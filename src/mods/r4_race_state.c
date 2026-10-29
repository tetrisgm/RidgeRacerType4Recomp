/* Race-state sampler for R4 (see r4_race_state.h). Reads guest RAM only and
 * never changes the race itself. Updated on entry to the view build
 * (0x8006E4D0, once per view and frame) and read by the render thread. */
#include <math.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpu_state.h"
#include "mod_plugins.h"
#include "r4_race_state.h"
#include "packet_metadata.h"
#define R4_RACE_METADATA UINT64_C(0x5234524143450001)

/* The car effects are the only reader, so the hook belongs to their plugin. */
#define PLUGIN_ID "r4.car-effects"

#define VIEW_BUILD 0x8006E4D0u
#define FRAMES 0x800AC064u
#define RACE_WORD 0x800AC794u           /* u16, 0x180 in a race */
#define CAR_TABLE 0x800FFDD0u           /* pointers */
#define CAR_COUNT 0x800AC754u           /* s16 */
#define CAM_EYE 0x1F800008u
#define CAM_PITCH 0x1F800018u
#define CAM_YAW 0x1F80001Cu
#define CAM_ROLL 0x1F800020u
#define CURRENT_VIEW 0x800AC790u
#define VIEW_OFX 0x800A3D5Cu
#define VIEW_OFY 0x800A3D6Cu
#define VIEW_H 0x800A3D7Cu
#define LIGHT 0x800AC088u
#define VBLANKS 0x800A6548u          /* libgpu VSync counter (VSync(-1), 0x8008B330) */

static R4RaceState s_states[2];
static atomic_uint s_current;      /* index of the published state */
static atomic_uint s_version;      /* odd: write in progress */
static int s_published;
/* Full states of the last frames (the game draws frame N while the screen
 * shows N - 1): r4_race_state_read_delayed returns the one on screen. */
#define HISTORY 4
static R4RaceState s_history[HISTORY];
static unsigned s_history_n;

static int16_t r16(uint32_t a) { return (int16_t)psx_mod_read_half(a); }
static int32_t r32(uint32_t a) { return (int32_t)psx_mod_read_word(a); }

/* Signed b - a on a 4096 circle, as 0x80025F78(a, b) with the sign of b - a. */
static int32_t angle_diff(int32_t a, int32_t b)
{
    int32_t d = (b - a) & 0xFFF;
    return d >= 0x800 ? d - 0x1000 : d;
}

static void rotations(float m[3][3], int32_t pitch, int32_t yaw, int32_t roll)
{
    const float k = 6.28318531f / 4096.f;
    const float sy = sinf(yaw * k), cy = cosf(yaw * k);
    const float sx = sinf(pitch * k), cx = cosf(pitch * k);
    const float sz = sinf(roll * k), cz = cosf(roll * k);
    /* 0x8006E3C0 (Y), 0x8006E42C (X), 0x8006E354 (Z), products 0x80090F90: M = Z*X*Y */
    const float Y[3][3] = {{cy, 0, -sy}, {0, 1, 0}, {sy, 0, cy}};
    const float X[3][3] = {{1, 0, 0}, {0, cx, -sx}, {0, sx, cx}};
    const float Z[3][3] = {{cz, -sz, 0}, {sz, cz, 0}, {0, 0, 1}};
    float XY[3][3];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            XY[i][j] = X[i][0] * Y[0][j] + X[i][1] * Y[1][j] + X[i][2] * Y[2][j];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            m[i][j] = Z[i][0] * XY[0][j] + Z[i][1] * XY[1][j] + Z[i][2] * XY[2][j];
}

static void sample_car(R4RaceCar *v, const R4RaceCar *prev, uint32_t a, int new_frame)
{
    const int32_t gear = r16(a + 0x27Au), speed = r16(a + 0x1D8u);
    v->x = r32(a + 0x10u); v->y = r32(a + 0x14u); v->z = r32(a + 0x18u);
    v->heading = r32(a + 0x54u) & 0xFFF;
    v->pitch = r32(a + 0x50u) & 0xFFF;
    v->roll = r32(a + 0x58u) & 0xFFF;
    v->motion_heading = r16(a + 0x1D4u) & 0xFFF;
    v->slip = angle_diff(v->heading, v->motion_heading);
    v->speed = speed;
    v->vx = r32(a + 0x30u); v->vz = r32(a + 0x38u);
    v->rpm = r32(a + 0x27Cu);
    v->gear = gear;
    v->rank = r16(a + 0x1EEu);
    v->lap = r16(a + 0x2AAu);

    v->since_shift = prev->since_shift;
    v->shift_dir = prev->shift_dir;
    v->since_impact = prev->since_impact;
    v->impact_loss = prev->impact_loss;
    if (!new_frame) return;
    if (v->since_shift >= 0) v->since_shift++;
    if (v->since_impact >= 0) v->since_impact++;
    if (prev->gear > 0 && gear > 0 && gear != prev->gear) {
        v->since_shift = 0;
        v->shift_dir = gear > prev->gear ? 1 : -1;
    }
    /* Impact: more than an eighth of the speed lost in one frame (braking
     * loses far less). */
    /* Position jump (savestate loaded, new race): no event. */
    const int64_t dx = (int64_t)v->x - prev->x, dz = (int64_t)v->z - prev->z;
    if (dx * dx + dz * dz > (int64_t)4000 * 4000) {
        v->since_shift = v->since_impact = -1;
        return;
    }
    const int32_t loss = prev->speed - speed;
    if (prev->speed > 200 && loss * 8 > prev->speed) {
        v->since_impact = 0;
        v->impact_loss = loss;
    }
}

/* ------------------------------------------------------------------ lab: free camera
 * Debug command "r4.camera" (TCP mod_cmd): view 0's camera is replaced on
 * entry to the view build (scratchpad inputs), so the game draws the scene
 * from that point.
 *   orbit CAR DISTANCE_M AZIMUTH_DEG ELEVATION_DEG [AIM_HEIGHT_M]
 *       around a car, aimed at its origin (azimuth 0: behind, 90: right, 180: front)
 *   freeze                 keep the last orbit camera, fixed in the world
 *   off                    give the camera back to the game
 *   state                  camera and cars (world positions, heading) */
static atomic_int s_lab_mode;            /* 0: game, 1: orbit, 2: frozen */
static atomic_int s_lab_car, s_lab_dist, s_lab_az, s_lab_el, s_lab_aim;   /* mm, millidegrees */

static int32_t s_frozen[5];               /* last orbit camera: eye x y z, pitch, yaw */

static void lab_camera(void)
{
    if (atomic_load(&s_lab_mode) == 2) {   /* frozen in the world (the car drives past) */
        for (int j = 0; j < 3; ++j) psx_mod_write_word(CAM_EYE + 4u * (uint32_t)j, (uint32_t)s_frozen[j]);
        psx_mod_write_word(CAM_PITCH, (uint32_t)(s_frozen[3] & 0xFFF));
        psx_mod_write_word(CAM_YAW, (uint32_t)(s_frozen[4] & 0xFFF));
        psx_mod_write_word(CAM_ROLL, 0u);
        return;
    }
    if (atomic_load(&s_lab_mode) != 1) return;
    int i = atomic_load(&s_lab_car);
    if (i < 0 || i >= R4_RACE_CARS) return;
    const uint32_t a = (uint32_t)r32(CAR_TABLE + 4u * (uint32_t)i);
    if ((a & 0xFFE00000u) != 0x80000000u) return;
    const float k = 6.28318531f / 4096.f, deg = 0.0174532925f / 1000.f;
    const float heading = (float)(r32(a + 0x54u) & 0xFFF) * k;
    const float d = atomic_load(&s_lab_dist) / 1000.f * R4_METER;
    const float az = atomic_load(&s_lab_az) * deg, el = atomic_load(&s_lab_el) * deg;
    const float aim = atomic_load(&s_lab_aim) / 1000.f * R4_METER;
    const float fx = sinf(heading), fz = cosf(heading);        /* car forward */
    const float rx = cosf(heading), rz = -sinf(heading);       /* car right */
    const float fwd = -cosf(az) * cosf(el), right = sinf(az) * cosf(el);
    const float cx = (float)r32(a + 0x10u), cy = (float)r32(a + 0x14u) - aim, cz = (float)r32(a + 0x18u);
    const float ex = cx + (fx * fwd + rx * right) * d, ez = cz + (fz * fwd + rz * right) * d;
    const float ey = cy - sinf(el) * d;                        /* y down */
    float dx = cx - ex, dy = cy - ey, dz = cz - ez;
    const float n = sqrtf(dx * dx + dy * dy + dz * dz);
    if (n < 1.f) return;
    dx /= n; dy /= n; dz /= n;
    /* camera forward = row 3 of Z*X*Y = (cos p * sin y, sin p, cos p * cos y) */
    const int32_t pitch = (int32_t)lrintf(asinf(dy) / k), yaw = (int32_t)lrintf(atan2f(dx, dz) / k);
    psx_mod_write_word(CAM_EYE, (uint32_t)(int32_t)lrintf(ex));
    psx_mod_write_word(CAM_EYE + 4u, (uint32_t)(int32_t)lrintf(ey));
    psx_mod_write_word(CAM_EYE + 8u, (uint32_t)(int32_t)lrintf(ez));
    psx_mod_write_word(CAM_PITCH, (uint32_t)(pitch & 0xFFF));
    psx_mod_write_word(CAM_YAW, (uint32_t)(yaw & 0xFFF));
    psx_mod_write_word(CAM_ROLL, 0u);
    s_frozen[0] = (int32_t)lrintf(ex); s_frozen[1] = (int32_t)lrintf(ey); s_frozen[2] = (int32_t)lrintf(ez);
    s_frozen[3] = pitch; s_frozen[4] = yaw;
}

static int camera_command(const char *args, char *reply, int cap)
{
    char word[16] = "";
    sscanf(args, "%15s", word);
    if (!strcmp(word, "orbit")) {
        int v = 0; float d = 6.f, az = 0.f, el = 15.f, aim = 0.f;
        if (sscanf(args, "%*s %d %f %f %f %f", &v, &d, &az, &el, &aim) < 1) {
            snprintf(reply, (size_t)cap, "orbit CAR DISTANCE_M AZIMUTH_DEG ELEVATION_DEG [AIM_M]");
            return 0;
        }
        atomic_store(&s_lab_car, v);
        atomic_store(&s_lab_dist, (int)(d * 1000.f));
        atomic_store(&s_lab_az, (int)(az * 1000.f));
        atomic_store(&s_lab_el, (int)(el * 1000.f));
        atomic_store(&s_lab_aim, (int)(aim * 1000.f));
        atomic_store(&s_lab_mode, 1);
        snprintf(reply, (size_t)cap, "orbit car %d, %.1f m, azimuth %.0f, elevation %.0f", v, d, az, el);
        return 1;
    }
    if (!strcmp(word, "freeze")) {
        atomic_store(&s_lab_mode, 2);
        snprintf(reply, (size_t)cap, "camera frozen %d %d %d", s_frozen[0], s_frozen[1], s_frozen[2]);
        return 1;
    }
    if (!strcmp(word, "off")) { atomic_store(&s_lab_mode, 0); snprintf(reply, (size_t)cap, "game camera"); return 1; }
    if (!strcmp(word, "state")) {
        R4RaceState s;
        if (!r4_race_state_read(&s)) { snprintf(reply, (size_t)cap, "no state"); return 0; }
        int k = snprintf(reply, (size_t)cap, "frame %u race %d eye %d %d %d p %d y %d r %d h %d (table %d)",
                         s.frame, s.in_race, s.cameras[0].eye[0], s.cameras[0].eye[1],
                         s.cameras[0].eye[2], s.cameras[0].pitch, s.cameras[0].yaw, s.cameras[0].roll,
                         s.cameras[0].h, s.cameras[0].h_table);
        for (int i = 0; i < s.car_count && k < cap - 80; ++i)
            k += snprintf(reply + k, (size_t)(cap - k), " | car %d: %d %d %d heading %d speed %d", i,
                          s.cars[i].x, s.cars[i].y, s.cars[i].z, s.cars[i].heading, s.cars[i].speed);
        return 1;
    }
    snprintf(reply, (size_t)cap, "orbit | freeze | off | state");
    return 0;
}

/* ------------------------------------------------------------------ stabilized camera
 * The game camera is integer: eye in world units, angles in 4096ths of a turn.
 * Crawling on the grid it follows the vibrating body and swings by one angle
 * step from frame to frame: 0.09 degrees, half a pixel at 240 lines but
 * 3 pixels at 1680, so the whole image shakes. Hysteresis of one step
 * (angles) and a few units (eye, 5 cm): a one-step back-and-forth is ignored,
 * a real move passes with this constant (invisible) lag; a jump (camera
 * change) passes as is. Visual only: the filtered camera is written to the
 * scratchpad on entry to the view build, before the game derives the GTE
 * matrix from it. PSX_R4_CAMERA_STABLE=0 turns it off. */
#define STABLE_EYE 6
#define STABLE_ANGLE 1
#define CUT_EYE 1500
#define CUT_ANGLE 128
#define STABLE_VIEWS 8                         /* game view indices (6 in a race) */

static int32_t s_stable[STABLE_VIEWS][6];    /* eye x y z, pitch, yaw, roll (previous output) */
static int s_stable_ok[STABLE_VIEWS];

static int32_t follow(int32_t prev, int32_t diff, int32_t in, int32_t threshold, int32_t cut)
{
    if (diff > cut || diff < -cut) return in;
    if (diff <= threshold && diff >= -threshold) return prev;
    return diff > 0 ? in - threshold : in + threshold;
}

static void stabilize_camera(int view)
{
    static int read_env, enabled = 1;
    if (!read_env) { read_env = 1; const char *e = getenv("PSX_R4_CAMERA_STABLE"); enabled = !(e && e[0] == '0'); }
    if (!enabled || view < 0 || view >= STABLE_VIEWS || atomic_load(&s_lab_mode) != 0) return;
    static const uint32_t addr[6] = {CAM_EYE, CAM_EYE + 4u, CAM_EYE + 8u, CAM_PITCH, CAM_YAW, CAM_ROLL};
    int32_t in[6], *p = s_stable[view];
    for (int i = 0; i < 6; ++i) in[i] = r32(addr[i]);
    if (!s_stable_ok[view]) { memcpy(p, in, sizeof in); s_stable_ok[view] = 1; return; }
    for (int i = 0; i < 6; ++i) {
        int32_t out;
        if (i < 3) out = follow(p[i], in[i] - p[i], in[i], STABLE_EYE, CUT_EYE);
        else {
            out = follow(p[i], angle_diff(p[i], in[i]), in[i], STABLE_ANGLE, CUT_ANGLE);
            if ((uint32_t)in[i] < 0x1000u) out &= 0xFFF;   /* angle kept within one turn: keep it there */
        }
        if (out != in[i]) psx_mod_write_word(addr[i], (uint32_t)out);
        p[i] = out;
    }
}

static void view_build(struct CPUState *cpu, uint32_t address)
{
    (void)address;
    psx_mod_counter_add("r4.race.view", 1);
    { const int v = r16(CURRENT_VIEW); if (v < 0 || v >= R4_RACE_VIEWS || v == 0) lab_camera(); }
    stabilize_camera(r16(CURRENT_VIEW));
    const unsigned cur = atomic_load_explicit(&s_current, memory_order_relaxed);
    const R4RaceState *prev = &s_states[cur];
    R4RaceState *e = &s_states[cur ^ 1u];
    const uint32_t frame = (uint32_t)r32(FRAMES);
    const int new_frame = !s_published || frame != prev->frame;

    atomic_fetch_add_explicit(&s_version, 1u, memory_order_acq_rel);
    if (!s_published) memset(e, 0, sizeof *e);
    else *e = *prev;
    if (s_published && new_frame) s_history[s_history_n++ % HISTORY] = *prev;
    e->frame = frame;
    e->in_race = psx_mod_read_half(RACE_WORD) == 0x180u;

    if (new_frame) {
        e->vblank = (uint32_t)r32(VBLANKS);
        int n = r16(CAR_COUNT);
        if (n < 1) n = 1;
        if (n > R4_RACE_CARS) n = R4_RACE_CARS;
        e->car_count = n;
        for (int i = 0; i < n; ++i) {
            const uint32_t a = (uint32_t)r32(CAR_TABLE + 4u * (uint32_t)i);
            if ((a & 0xFFE00000u) != 0x80000000u) continue;
            R4RaceCar empty;
            memset(&empty, 0, sizeof empty);
            empty.since_shift = empty.since_impact = -1;
            sample_car(&e->cars[i], s_published ? &prev->cars[i] : &empty, a, s_published);
        }
        /* dominant light (row 0 of the world light matrix) */
        const float lx = r16(LIGHT), ly = r16(LIGHT + 2u), lz = r16(LIGHT + 4u);
        const float n_l = sqrtf(lx * lx + ly * ly + lz * lz);
        if (n_l > 1.f) { e->light_dir[0] = lx / n_l; e->light_dir[1] = ly / n_l; e->light_dir[2] = lz / n_l; }
    }

    int view = r16(CURRENT_VIEW);
    if (view < 0 || view >= R4_RACE_VIEWS) view = 0;
    R4RaceCamera *c = &e->cameras[view];
    c->valid = 1;
    for (int i = 0; i < 3; ++i) c->eye[i] = r32(CAM_EYE + 4u * (uint32_t)i);
    c->pitch = r32(CAM_PITCH);
    c->yaw = r32(CAM_YAW);
    c->roll = r32(CAM_ROLL);
    rotations(c->matrix, c->pitch, c->yaw, c->roll);
    c->ofx = r16(VIEW_OFX + 2u * (uint32_t)view);
    c->ofy = r16(VIEW_OFY + 2u * (uint32_t)view);
    c->h = r32(VIEW_H + 4u * (uint32_t)view);
    /* focal length actually loaded in the GTE (H, control register 26): the
     * demo's replay cameras change it right before the view build (0x80056734) */
    { const int32_t h = (int32_t)(cpu->gte_ctrl[26] & 0xFFFFu); if (h > 16) c->h = h; c->h_table = r32(VIEW_H + 4u * (uint32_t)view); }

    atomic_store_explicit(&s_current, cur ^ 1u, memory_order_release);
    atomic_fetch_add_explicit(&s_version, 1u, memory_order_acq_rel);
    s_published = 1;
}

static const R4RaceState *state_at(int delay)
{
    if (delay <= 0) return &s_states[atomic_load_explicit(&s_current, memory_order_acquire)];
    if (delay > HISTORY - 1) delay = HISTORY - 1;
    const unsigned n = s_history_n;
    if (n < (unsigned)delay) return &s_states[atomic_load_explicit(&s_current, memory_order_acquire)];
    return &s_history[(n - (unsigned)delay) % HISTORY];
}

int r4_race_state_read_delayed(R4RaceState *out, int delay)
{
    if(packet_metadata_read(R4_RACE_METADATA,out,sizeof *out)) return 1;
    for (int attempt = 0; attempt < 4; ++attempt) {
        const unsigned v0 = atomic_load_explicit(&s_version, memory_order_acquire);
        if (!v0) return 0;
        if (v0 & 1u) continue;
        *out = *state_at(delay);
        if (atomic_load_explicit(&s_version, memory_order_acquire) == v0) return 1;
    }
    /* Continuous writes: the published state stays readable (the writer
     * fills the other one). */
    *out = *state_at(delay);
    return 1;
}

int r4_race_state_read(R4RaceState *out) { return r4_race_state_read_delayed(out, 0); }

int r4_race_project(const R4RaceState *s, int view, float wx, float wy, float wz,
                    float *sx, float *sy, float *depth)
{
    if (view < 0 || view >= R4_RACE_VIEWS) return 0;
    const R4RaceCamera *k = &s->cameras[view];
    if (!k->valid || k->h <= 0) return 0;
    const float d[3] = {(wx - (float)k->eye[0]) / 4.f, (wy - (float)k->eye[1]) / 4.f,
                        (wz - (float)k->eye[2]) / 4.f};
    float p[3];
    for (int i = 0; i < 3; ++i) p[i] = k->matrix[i][0] * d[0] + k->matrix[i][1] * d[1] + k->matrix[i][2] * d[2];
    if (p[2] < 1.f) return 0;
    *sx = (float)k->ofx + (float)k->h * p[0] / p[2];
    *sy = (float)k->ofy + (float)k->h * p[1] / p[2];
    if (depth) *depth = p[2];
    return 1;
}

static int packet_capture(void *out) {
    memset(out,0,sizeof(R4RaceState));
    (void)r4_race_state_read_delayed(out,0);
    return 1; /* An empty captured state must not fall back to live guest state. */
}
static int32_t lerp_i(int32_t a,int32_t b,double t) {return (int32_t)llround(a+t*((double)b-a));}
static int32_t lerp_angle(int32_t a,int32_t b,double t) {return (a+(int32_t)llround(t*angle_diff(a,b)))&4095;}
static void packet_blend(const void *older,const void *newer,double t,void *output) {
    const R4RaceState *a=older,*b=newer;R4RaceState *o=output;*o=*b;
    if(!a->in_race || !b->in_race || a->car_count!=b->car_count) return;
    for(int i=0;i<b->car_count && i<R4_RACE_CARS;++i) {
        const R4RaceCar *ac=&a->cars[i],*bc=&b->cars[i];R4RaceCar *c=&o->cars[i];
#define LINEAR(f) c->f=lerp_i(ac->f,bc->f,t)
#define ANGLE(f) c->f=lerp_angle(ac->f,bc->f,t)
        LINEAR(x);LINEAR(y);LINEAR(z);LINEAR(vx);LINEAR(vz);LINEAR(speed);LINEAR(rpm);LINEAR(slip);
        ANGLE(heading);ANGLE(pitch);ANGLE(roll);ANGLE(motion_heading);
#undef LINEAR
#undef ANGLE
    }
    for(int i=0;i<R4_RACE_VIEWS;++i) {
        const R4RaceCamera *ac=&a->cameras[i],*bc=&b->cameras[i];R4RaceCamera *c=&o->cameras[i];
        if(!ac->valid || !bc->valid) continue;
        for(int k=0;k<3;++k)c->eye[k]=lerp_i(ac->eye[k],bc->eye[k],t);
        c->pitch=lerp_angle(ac->pitch,bc->pitch,t);c->yaw=lerp_angle(ac->yaw,bc->yaw,t);
        c->roll=lerp_angle(ac->roll,bc->roll,t);rotations(c->matrix,c->pitch,c->yaw,c->roll);
        c->h=lerp_i(ac->h,bc->h,t);c->ofx=lerp_i(ac->ofx,bc->ofx,t);c->ofy=lerp_i(ac->ofy,bc->ofy,t);
    }
    for(int k=0;k<3;++k)o->light_dir[k]=(float)(a->light_dir[k]+t*(b->light_dir[k]-a->light_dir[k]));
}

PSX_MOD_CONSTRUCTOR(r4_register_race_state)
{
    (void)packet_metadata_register(R4_RACE_METADATA,sizeof(R4RaceState),packet_capture,packet_blend);
    (void)psx_mod_register_function_entry_plugin(PLUGIN_ID, VIEW_BUILD, view_build);
    (void)psx_mod_register_command("r4.camera", camera_command);
}

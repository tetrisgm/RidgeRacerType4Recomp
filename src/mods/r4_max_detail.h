/* r4_max_detail.h - pure helpers for r4.enhancement.max-detail (R4 Max
 * Detail). docs/MAX_DETAIL.md has the research behind every value.
 *
 * Every frame R4 lowers detail in four places this mod can lift safely:
 *
 *   - Course "mipmapping": each course polygon past a depth threshold
 *     (scratch 0x1F80005C, rewritten every frame, -1 = everything in the
 *     rear-view mirror) is drawn flat-shaded with a half-resolution copy of
 *     its texture. Course detail = full writes 0x7FFF there at the course
 *     renderer's entry, so every polygon takes the full path.
 *   - Split-screen subdivision: the VS race sets scratch 0x1F80005E before
 *     each course draw, which selects coarser subdivision thresholds than 1P.
 *     Split screen = same as 1P writes 0 there at the same entry.
 *   - Car models: the table at 0x8009F228 gives per-view distances for three
 *     model levels and the cull distance. Car detail = full keeps the full
 *     model (3D wheels, full-resolution texture) out to the cull distance;
 *     the manifest's guarded [[patch]] entries write it. The cull distance
 *     itself (8704) is a correctness limit and never changes.
 *   - Draw distance: the course renderers drop a polygon past the end of the
 *     ordering table instead of drawing it. game.toml lists those 18 guards
 *     as [[draw_distance.clamp]] sites; the plugin switches psxrecomp's clamp
 *     on so far polygons land in the farthest slot. Maximum also adds the
 *     neighbouring track sections' visibility lists (r4_pvs.h) up to about
 *     30:9.
 *
 * Two more options are not distance reducers:
 *
 *   - Car reflections: the game draws car bodies with an environment map
 *     (the reflective look of replays) whenever the texture page at
 *     0x8009E288 is >= 0. It sets 10 at race init (the fly-by), after the
 *     goal and in every replay and attract-demo frame, and -1 at the start
 *     signal of every live race. Car reflections = on puts 10 back during
 *     the live race (on by default, owner decision 2026-10-01), in views
 *     without a widescreen margin: in psxrecomp's native-wide renderer the
 *     reflective parts cost the host many times their 4:3 cost (see
 *     r4_md_reflections_action).
 *   - Mirror scenery: the rear-view mirror draws only the first few blocks
 *     of its course list (a per-section limit, 0x80071704). Full keeps the
 *     whole list. Off by default (owner decision 2026-10-01): it costs the
 *     emulated PS1 the most time of anything here.
 *
 * Both scratch values are rebuilt by the game every frame, so nothing the
 * plugin writes outlives a frame in which it did not run. The two exceptions
 * are the car table (manifest patches) and the reflection page, which keeps
 * 10 until the game next sets it (at the next race start, or by a menu).
 */
#ifndef R4_MAX_DETAIL_H
#define R4_MAX_DETAIL_H

#include <math.h>
#include <stdint.h>
#include <string.h>

/* ---- guest layout (US) --------------------------------------------------- */
#define R4_MD_COURSE_DISPATCH_FN 0x80060F94u   /* course renderer dispatcher */
#define R4_MD_COURSE_FAR_ADDR    0x1F80005Cu   /* s16 far-path depth threshold */
#define R4_MD_COURSE_2P_ADDR     0x1F80005Eu   /* s16 nonzero: 2P subdivision */
#define R4_MD_COURSE_FAR_FULL    0x7FFFu       /* no polygon takes the far path */
#define R4_MD_CAR_LOD_TABLE      0x8009F228u   /* 5 rows x (T0, T1, T2) s16 */
#define R4_MD_CAR_LOD_ROWS       5u
#define R4_MD_CAR_CULL           8704          /* T2: never raised */

/* Car reflections. 0x80014A90 is the only reader of the page word (twice,
 * 0x80014ABC and 0x80014AE8, with no store in between); the setter
 * 0x80014A84 is called with 10 by race init (0x8003D3E0), the after-goal
 * run (0x8002A3E4) and replays/attract (0x8005EB78), with 25 by the garage,
 * and with -1 when the race overlays raise the start signal (race phase
 * 0 -> 1: 0x80114D6C in Grand Prix, 0x80117510 in Time Attack, 0x801150C0
 * in VS; Extra Trial and link battle, overlays 666/667, likewise) and by
 * the menus. */
#define R4_MD_ENV_RENDER_FN      0x80014A90u   /* car part with the env map */
#define R4_MD_ENV_TPAGE_ADDR     0x8009E288u   /* s32 env texture page, -1 = off */
#define R4_MD_ENV_TPAGE_RACE     10u           /* what race init and replays set */

/* Rear-view mirror course list. The mirror draw 0x8006F0C8 builds the list
 * (0x8006EB58), calls the limit 0x80071704, lowers the list's count word to
 * the limit (0x8006F100-0x8006F114: only the count is written; the block
 * pointers past it stay), sets up the GTE (0x8006EF88) and hands the list to
 * its first consumer 0x8006EDEC. The plugin reads the built count at the
 * limit's entry and puts it back at the consumer's entry; both gate on the
 * mirror draw's return addresses. */
#define R4_MD_MIRROR_LIMIT_FN    0x80071704u
#define R4_MD_MIRROR_LIMIT_RA    0x8006F0FCu   /* after jal 0x80071704 at 0x8006F0F4 */
#define R4_MD_MIRROR_LIST_FN     0x8006EDECu
#define R4_MD_MIRROR_LIST_RA     0x8006F128u   /* after jal 0x8006EDEC at 0x8006F120 */
#define R4_MD_LIST_ADDR          0x8010E3C0u   /* count, then block pointers */
#define R4_MD_LIST_MAX           255u          /* 0x8006EB58 caps the list here */

/* Far object transform. 0x8006F160(out, pos, matrix) is called by every car,
 * trackside object and effect draw (34 call sites): it writes pos - camera
 * (camera at 0x1F800008, world >> 6, s32 x/y/z) to out+0 as an SVECTOR
 * (`sh`, so it wraps past 32767), rotates it by the camera matrix
 * (0x1F800028) with 0x800910A0 (MVMVA, sf = 1, MAC1-3 to out+8 as a
 * VECTOR), copies that to out+0x2C (the t[] of the MATRIX at out+0x18) and
 * calls SetRotMatrix(matrix), then SetTransMatrix(out+0x18) from
 * 0x8006F1E0. */
#define R4_MD_XF_FN              0x8006F160u
#define R4_MD_XF_SETTRANS_FN     0x80091320u   /* SetTransMatrix */
#define R4_MD_XF_SETTRANS_RA     0x8006F1E8u   /* after jal 0x80091320 at 0x8006F1E0 */
#define R4_MD_XF_VECTOR_OFF      0x08u
#define R4_MD_XF_MATRIX_OFF      0x18u
#define R4_MD_CAMERA_POS_ADDR    0x1F800008u
#define R4_MD_CAMERA_MATRIX_ADDR 0x1F800028u

/* The camera-space translation the fix writes is fed to RTPS/RTPT with the
 * model's vertices; the GTE's IR1/IR2 saturate at +/-32767 and SZ at 65535
 * (world >> 6). An object whose translation leaves that range (it is then
 * off screen, or past the depth any renderer keeps) is moved behind the
 * camera instead, so every renderer rejects it (SZ 0, OT index 0). */
#define R4_MD_XF_SAFE_XY     30000
#define R4_MD_XF_SAFE_Z      60000
#define R4_MD_XF_BEHIND      (-0x100000)
static inline int r4_md_xf_safe(const int32_t t[3])
{
    return t[0] >= -R4_MD_XF_SAFE_XY && t[0] <= R4_MD_XF_SAFE_XY &&
           t[1] >= -R4_MD_XF_SAFE_XY && t[1] <= R4_MD_XF_SAFE_XY && t[2] <= R4_MD_XF_SAFE_Z;
}

/* Does the delta fit the game's 16-bit SVECTOR unchanged? */
static inline int r4_md_delta_fits(const int32_t d[3])
{
    for (unsigned i = 0; i < 3u; i++)
        if (d[i] < -32768 || d[i] > 32767) return 0;
    return 1;
}

/* MVMVA with sf = 1, no translation: t = (m * d) >> 12 per row, on the
 * GTE's 44-bit accumulator (here 64-bit; the inputs cannot overflow it). */
static inline void r4_md_apply_matrix(const int16_t m[9], const int32_t d[3], int32_t t[3])
{
    for (unsigned r = 0; r < 3u; r++) {
        int64_t acc = (int64_t)m[3u * r] * d[0] + (int64_t)m[3u * r + 1u] * d[1] +
                      (int64_t)m[3u * r + 2u] * d[2];
        t[r] = (int32_t)(acc >> 12);
    }
}

/* ---- course blocks ------------------------------------------------------------
 * A course block (0x50 bytes, r4_pvs.h) holds, per kind of course polygon,
 * a word (count | first << 16) at `field`; that kind's polys live in the
 * table *`table`, `stride` bytes each (the 1P renderer chain 0x800A2370:
 * 0x80061078, 0x80061C34, ... each loads its own field, table pointer and
 * record size; the kind at +0x30, 0x80066D58, is drawn differently and is
 * left out). Every kind starts with the same packed vertices: x0|y0,
 * z0|z1, x1|y1, x2|y2, z2|z3, x3|y3 (s16), with world (>> 8) = v + 0x8000
 * for x and z (the course draw translates by 0x8000 - camera >> 2,
 * 0x8006EDEC). */
typedef struct { uint8_t field, stride; uint32_t table; } R4MdPolyKind;
static const R4MdPolyKind r4_md_poly_kinds[] = {
    { 0x10, 0x40, 0x800F2B90u }, { 0x14, 0x44, 0x800F4EE4u }, { 0x18, 0x30, 0x800ADC98u },
    { 0x1C, 0x34, 0x800F4ED8u }, { 0x20, 0x40, 0x800F4E10u }, { 0x24, 0x44, 0x800AC060u },
    { 0x28, 0x30, 0x800F4DD0u }, { 0x2C, 0x34, 0x800AC054u }, { 0x34, 0x34, 0x800F4DE0u },
    { 0x38, 0x38, 0x800AC058u },
};
#define R4_MD_POLY_KINDS (sizeof r4_md_poly_kinds / sizeof r4_md_poly_kinds[0])
#define R4_MD_BLOCK_POLYS_MAX 4096u

typedef uint32_t (*R4MdRead32)(uint32_t address);

/* A block's bounding circle on the ground plane (world >> 8): centre of the
 * x/z box and the half-diagonal. `sig` folds the block's kind words, so a
 * cached circle can be checked against the block it was made from. Returns
 * 0 for a block with no polygons or an implausible table. */
typedef struct { int32_t cx, cz, r; uint32_t sig; } R4MdBounds;

static inline uint32_t r4_md_block_sig(R4MdRead32 rd32, uint32_t block)
{
    uint32_t sig = 0x9E3779B9u;
    for (unsigned k = 0; k < R4_MD_POLY_KINDS; k++)
        sig = (sig ^ rd32(block + r4_md_poly_kinds[k].field)) * 0x01000193u;
    return sig;
}

static inline int r4_md_block_bounds(R4MdRead32 rd32, uint32_t block, R4MdBounds *b)
{
    int32_t x0 = INT32_MAX, x1 = INT32_MIN, z0 = INT32_MAX, z1 = INT32_MIN;
    for (unsigned k = 0; k < R4_MD_POLY_KINDS; k++) {
        uint32_t w = rd32(block + r4_md_poly_kinds[k].field);
        uint32_t polys = rd32(r4_md_poly_kinds[k].table);
        uint32_t n = w & 0xFFFFu, first = w >> 16;
        if (n == 0u || (polys & 0xFFE00000u) != 0x80000000u || n > R4_MD_BLOCK_POLYS_MAX)
            continue;
        for (uint32_t i = 0; i < n; i++) {
            uint32_t p = polys + r4_md_poly_kinds[k].stride * (first + i);
            uint32_t v[6];
            for (unsigned j = 0; j < 6u; j++) v[j] = rd32(p + 4u * j);
            int32_t xs[4] = { (int16_t)v[0], (int16_t)v[2], (int16_t)v[3], (int16_t)v[5] };
            int32_t zs[4] = { (int16_t)v[1], (int16_t)(v[1] >> 16), (int16_t)v[4],
                              (int16_t)(v[4] >> 16) };
            for (unsigned j = 0; j < 4u; j++) {
                if (xs[j] < x0) x0 = xs[j];
                if (xs[j] > x1) x1 = xs[j];
                if (zs[j] < z0) z0 = zs[j];
                if (zs[j] > z1) z1 = zs[j];
            }
        }
    }
    if (x0 > x1) return 0;
    b->cx = (x0 + x1) / 2 + 0x8000;
    b->cz = (z0 + z1) / 2 + 0x8000;
    int64_t hx = ((int64_t)x1 - x0 + 1) / 2 + 1, hz = ((int64_t)z1 - z0 + 1) / 2 + 1;
    b->r = (int32_t)sqrt((double)(hx * hx + hz * hz)) + 1;
    b->sig = r4_md_block_sig(rd32, block);
    return 1;
}

/* Far blocks appended at Maximum are kept only where they can be seen and
 * drawn correctly:
 *   - in view: the circle meets the horizontal view cone (|lateral| <=
 *     forward * slope, slope = tan of the half view angle plus a margin for
 *     roll and pitch), so the PS1 never transforms the polygons of a block
 *     behind or beside the camera;
 *   - in range: every point of the circle within R4_MD_BLOCK_RANGE of the
 *     camera. The course draw feeds camera-space vertices through the GTE's
 *     16-bit IR registers (RTPT, sf = 1): past 32767 they saturate and the
 *     polygon would be drawn bent. */
#define R4_MD_BLOCK_RANGE 30000
static inline int r4_md_block_visible(const R4MdBounds *b, double camx, double camz,
                                      double fx, double fz, double slope)
{
    double x = (double)b->cx - camx, z = (double)b->cz - camz, r = (double)b->r;
    double d2 = x * x + z * z;
    double lim = (double)R4_MD_BLOCK_RANGE - r;
    if (lim <= 0.0 || d2 > lim * lim) return 0;
    double fwd = x * fx + z * fz, lat = x * fz - z * fx;
    if (lat < 0) lat = -lat;
    /* The circle meets the cone when its centre is within r of it: the
     * distance from the centre to the cone's edge line is
     * (lat - fwd * slope) / sqrt(1 + slope^2). */
    if (fwd < -r) return 0;
    return (lat - fwd * slope) <= r * sqrt(1.0 + slope * slope);
}

/* The view cone's half-angle slope for a view `margin` pixels wider than
 * 320 on each side: the 4:3 view spans about +/-51 degrees (slope 1.25);
 * a wider view scales the half-width. Plus 0.35 for roll, pitch and the
 * camera moving within a frame. */
static inline double r4_md_view_slope(int margin)
{
    if (margin < 0) margin = 0;
    return 1.25 * (160.0 + (double)margin) / 160.0 + 0.35;
}

/* Far cars. 0x8002DC00(car, row) reads the row's three distances (T0 at
 * 0x8002DE34, T1 0x8002E29C, T2 0x8002E44C) against the car's L1 distance
 * ((car - camera) >> 2 on x and z, world >> 8) and draws the full model
 * below T0, a simpler one below T1, the simplest below T2, nothing past
 * it. Every path then calls 0x80015F60 (from the four return addresses
 * below), after the last read. The stock cull distance 8704 is the range
 * of 0x8006F160's 16-bit delta (8192 per axis) rounded up; with the far
 * transform fix the limit is the car renderer's own: its ordering-table
 * guard (0x8005F6BC: OTZ >> 5 below 447, about 14300 deep) and the GTE's
 * SZ (65535 >> 2, about 16380 deep). So at Extended and Maximum the plugin
 * raises the 1P, TV and 2P rows' cull distance to 14000 for the duration
 * of each car's lookup only: written at 0x8002DC00's entry and put back at
 * 0x80015F60's, so the table in RAM, and in any save state, stays what the
 * manifest wrote. The mirror row is left alone. */
#define R4_MD_CAR_DRAW_FN        0x8002DC00u
#define R4_MD_CAR_AFTER_LOD_FN   0x80015F60u
static const uint32_t r4_md_car_after_lod_ra[4] = {
    0x8002E1B8u,   /* full model (0x8002E1B0) */
    0x8002E3CCu,   /* simpler model (0x8002E3C4) */
    0x8002E4B4u,   /* simplest model (0x8002E4AC) */
    0x8002E524u,   /* past the cull distance (0x8002E51C) */
};
#define R4_MD_CAR_FAR            14000

/* Stock car rows: 1P, rear-view mirror, TV/replay, 2P, 2P (alternate). */
static const int16_t r4_md_car_lod_stock[R4_MD_CAR_LOD_ROWS][3] = {
    { 672, 3200, 8704 },
    { -1, 2560, 4096 },
    { 4096, 8192, 8704 },
    { 320, 3200, 8704 },
    { 160, 2560, 8704 },
};
/* Car detail = full: the full model to the cull distance, and the mirror
 * keeps its near model only up close but draws cars as far as ahead. */
static const int16_t r4_md_car_lod_full[R4_MD_CAR_LOD_ROWS][3] = {
    { 8704, 8704, 8704 },
    { 672, 8704, 8704 },
    { 8704, 8704, 8704 },
    { 8704, 8704, 8704 },
    { 8704, 8704, 8704 },
};

/* ---- options --------------------------------------------------------------- */
typedef enum {
    R4_MD_DRAW_STOCK = 0,
    R4_MD_DRAW_EXTENDED = 1,   /* far polygons kept in the farthest OT slot */
    R4_MD_DRAW_MAXIMUM = 2,    /* + neighbouring sections' visibility lists */
} R4MdDrawDistance;

typedef struct {
    R4MdDrawDistance draw;
    int course_full;   /* no far path: full textures and smooth shading */
    int cars_full;     /* the manifest's patches write the rows; Stock restores */
    int split_same;    /* 2P uses 1P's course subdivision */
    int reflections;   /* env-mapped car bodies in live races too */
    int mirror_full;   /* the rear-view mirror keeps its whole course list */
} R4MdOptions;

/* Manifest option values. A missing or unknown value takes the package's
 * default, so a stale state file cannot switch a default off by accident:
 * the most detail for the four distance options and Car reflections, Stock
 * for Mirror scenery (off by default). */
static inline R4MdDrawDistance r4_md_parse_draw(const char *v)
{
    if (v && strcmp(v, "stock") == 0) return R4_MD_DRAW_STOCK;
    if (v && strcmp(v, "extended") == 0) return R4_MD_DRAW_EXTENDED;
    return R4_MD_DRAW_MAXIMUM;
}
static inline int r4_md_parse_full(const char *v)
{
    return !(v && strcmp(v, "stock") == 0);
}
static inline int r4_md_parse_mirror(const char *v)
{
    return v && strcmp(v, "full") == 0;
}

static inline R4MdOptions r4_md_options(const char *draw, const char *course,
                                        const char *cars, const char *split,
                                        const char *reflections, const char *mirror)
{
    R4MdOptions o;
    o.draw = r4_md_parse_draw(draw);
    o.course_full = r4_md_parse_full(course);
    o.cars_full = r4_md_parse_full(cars);
    o.split_same = r4_md_parse_full(split);
    o.reflections = r4_md_parse_full(reflections);
    o.mirror_full = r4_md_parse_mirror(mirror);
    return o;
}

/* psxrecomp's draw-distance clamp: on from Extended. */
static inline int r4_md_clamp_on(R4MdOptions o) { return o.draw != R4_MD_DRAW_STOCK; }

/* The far object transform fix runs with any draw distance above Stock (the
 * only settings that raise the car cull distance). */
static inline int r4_md_far_xform_on(R4MdOptions o) { return o.draw != R4_MD_DRAW_STOCK; }

/* The row values to use for one car lookup: `cur` as the table holds it,
 * `out` what the lookup should see. Returns 0 when nothing changes. */
static inline int r4_md_car_row_far(R4MdOptions o, unsigned row, const int16_t cur[3],
                                    int16_t out[3])
{
    out[0] = cur[0]; out[1] = cur[1]; out[2] = cur[2];
    if (o.draw == R4_MD_DRAW_STOCK || row >= R4_MD_CAR_LOD_ROWS || row == 1u) return 0;
    if (cur[2] != R4_MD_CAR_CULL) return 0;   /* not a row this package knows */
    /* Only the cull distance moves: past the old one (where stock draws no
     * car at all) a car takes the game's simplest model, a few pixels tall
     * there; the full model's eight parts would cost the PS1 frame time
     * the course needs. */
    out[2] = R4_MD_CAR_FAR;
    return 1;
}

/* ---- Maximum: reach within the PS1's frame budget ---------------------------
 * Every block and car Maximum adds costs the emulated PS1 time, and R4 has
 * two VBlanks per game frame: past them the game drops a frame (it runs
 * its logic once per frame, so the race slows). The cost depends on the
 * course, the cars in view and the view's width, so a fixed reach is either
 * short everywhere or too long on a crowded grid. Maximum therefore
 * measures each frame's busy time, in guest cycles from the main loop's
 * frame start (0x8009375C, called at 0x8001E7E8) to its first VSync(1)
 * (0x8008B330 from 0x8001E7C8: the handler, DrawSync and the course and car
 * draws all come before it), and steers a level:
 *   level -2..10: sections ahead = 2 + level, one section behind (none at
 *   -2), far cars (cull distance 14000) from level 1.
 * Over 96 % of the budget the level drops by 2 and holds for 60 frames; over
 * 93 % by 1 (hold 30); after 20 frames in a row under 86 % it rises by 1.
 * Each level changes the busy time by a few percent, so the level settles
 * where the frame uses 86-93 % of its budget.
 * Guest cycles are deterministic, so from a save state (which resets the
 * level) the same input gives the same frames. */
#define R4_MD_FRAME_START_FN   0x8009375Cu   /* called at 0x8001E7E8 after VSync(0) */
#define R4_MD_FRAME_START_RA   0x8001E7F0u
#define R4_MD_VSYNC_FN         0x8008B330u
#define R4_MD_VSYNC_WAIT_RA    0x8001E7D0u   /* the VSync(1) floor loop at 0x8001E7C8 */
#define R4_MD_FRAME_BUDGET     1130090u      /* two NTSC VBlanks: 2 * 33868800 / 59.94 */
#define R4_MD_LEVEL_MIN        (-2)
#define R4_MD_LEVEL_MAX        10
#define R4_MD_BEHIND           1

typedef struct {
    int level;
    int calm;   /* frames in a row under the rise threshold */
    int hold;   /* frames before the level may rise again */
} R4MdGovernor;

static inline void r4_md_gov_reset(R4MdGovernor *g, int level)
{
    g->level = level < R4_MD_LEVEL_MIN ? R4_MD_LEVEL_MIN
             : level > R4_MD_LEVEL_MAX ? R4_MD_LEVEL_MAX : level;
    g->calm = 0;
    g->hold = 0;
}

/* One measured frame: `busy` guest cycles against R4_MD_FRAME_BUDGET.
 * Returns the level change (-2..+1). */
static inline int r4_md_gov_update(R4MdGovernor *g, uint64_t busy)
{
    uint64_t permille = busy * 1000u / R4_MD_FRAME_BUDGET;
    int before = g->level;
    if (permille > 960u) {
        g->level -= 2;
        g->hold = 60;
        g->calm = 0;
    } else if (permille > 930u) {
        g->level -= 1;
        if (g->hold < 30) g->hold = 30;
        g->calm = 0;
    } else if (permille < 860u) {
        if (g->hold > 0) g->hold--;
        if (++g->calm >= 20 && g->hold == 0) {
            g->level++;
            g->calm = 0;
        }
    } else {
        if (g->hold > 0) g->hold--;
        g->calm = 0;
    }
    if (g->level < R4_MD_LEVEL_MIN) g->level = R4_MD_LEVEL_MIN;
    if (g->level > R4_MD_LEVEL_MAX) g->level = R4_MD_LEVEL_MAX;
    return g->level - before;
}

/* The starting level for a view `oct_reach` octants wide (r4_pvs.h): the
 * widest views (about 30:9 on) start with no sections added, as before the
 * governor (their wide octants made two sections overrun the grid). */
static inline int r4_md_gov_start_level(int oct_reach)
{
    return oct_reach >= 2 ? R4_MD_LEVEL_MIN : 0;
}

static inline int r4_md_level_ahead(int level)
{
    int a = 2 + level;
    return a < 0 ? 0 : a;
}
static inline int r4_md_level_behind(int level)
{
    return r4_md_level_ahead(level) > 0 ? R4_MD_BEHIND : 0;
}
static inline int r4_md_level_far_cars(int level) { return level >= 1; }

/* Does the course renderer's entry hook have anything to write? */
static inline int r4_md_course_hook_active(R4MdOptions o)
{
    return o.course_full || o.split_same;
}

/* Car reflections, at the env-map draw's entry: what to write to the page
 * word, which holds `page`. +1 = R4_MD_ENV_TPAGE_RACE, -1 = put the game's
 * "off" (-1) back, 0 = nothing.
 *   live  - a live race: a race handler resident (the widescreen plugin's race
 *           predicate, r4_widescreen_scene.h) and the race phase 1..3
 *           (countdown and racing). The fly-by (phase 0) and everything from
 *           the finish on (4+) show the game's own reflections, the menus set
 *           -1, and only race VRAM is known to hold the environment map at
 *           page 10, so outside a live race nothing is written. The predicate
 *           knows the Grand Prix, Time Attack and VS handlers; Extra Trial and
 *           link battle (overlays 666/667) keep stock reflections, as they
 *           keep 4:3 in the widescreen plugin.
 *   wide  - psxrecomp's native-wide renderer is drawing (widescreen margin >
 *           0). The reflective parts toggle the GPU's set-mask bit (GP0 E6h)
 *           around every part, about 400 times a frame on the Grand Prix
 *           grid, which splits the renderer's textured batches, and in the
 *           native-wide renderer those extra batches are very expensive: at
 *           4K the host's scene GPU time went from 15-16 to 143-166 ms a frame,
 *           where 4:3 shows no difference (docs/MAX_DETAIL.md). The stock
 *           attract demo, which shows reflections, stalls the same way. The
 *           wide view's extra geometry plus reflections also overruns the
 *           PS1's own budget at the race start (28 game frames/s on the
 *           Helter Skelter grid at 16:9). So reflections stay stock in wide
 *           views, and turning a window wide mid-race puts back the -1 this
 *           plugin replaced.
 *   wrote - this plugin wrote the race page and the game has not set the page
 *           since (the plugin clears it when it sees -1 or leaves the race).
 * A page the game set itself (10 in the fly-by, after the goal and in replays,
 * 25 in the garage) is never touched. */
#define R4_MD_ENV_WRITE_ON   1
#define R4_MD_ENV_WRITE_OFF  (-1)
static inline int r4_md_reflections_action(R4MdOptions o, int live, int wide,
                                           uint32_t page, int wrote)
{
    if (!o.reflections || !live) return 0;
    if ((int32_t)page < 0) return wide ? 0 : R4_MD_ENV_WRITE_ON;
    return (wrote && wide) ? R4_MD_ENV_WRITE_OFF : 0;
}

/* Mirror scenery: the count word to put back at the mirror list's first
 * consumer, given the count 0x8006EB58 built (`built`, read at the limit's
 * entry) and the count the limit left (`now`). Stock, a count the limit did
 * not lower, or an implausible one leave `now`. */
static inline uint32_t r4_md_mirror_count(R4MdOptions o, uint32_t built, uint32_t now)
{
    if (!o.mirror_full || built > R4_MD_LIST_MAX || built <= now) return now;
    return built;
}

/* ---- car table after a save state --------------------------------------------
 * The car rows are written once by the manifest's [[patch]] entries, so a save
 * state carries whatever table it was made with, and loading one re-applies
 * only the current plan's writes. With Car detail = Stock the plan writes no
 * full rows, so the plugin puts the stock row back wherever it finds a row
 * this package could have left there (stock, Always full, or the 1P row that
 * Split screen = same copies into the 2P rows). Rows the current plan writes
 * itself (the 2P rows with Split screen = same) are left to the plan: the
 * plugin must not touch them before the plan's own guard check at the EXE
 * entry. A row it does not recognise is not the US table and stays untouched. */
static inline int r4_md_car_row_eq(const int16_t a[3], const int16_t b[3])
{
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
}

static inline int r4_md_car_row_known(unsigned row, const int16_t cur[3])
{
    if (row >= R4_MD_CAR_LOD_ROWS) return 0;
    return r4_md_car_row_eq(cur, r4_md_car_lod_stock[row]) ||
           r4_md_car_row_eq(cur, r4_md_car_lod_full[row]) ||
           ((row == 3u || row == 4u) && r4_md_car_row_eq(cur, r4_md_car_lod_stock[0]));
}

/* Should the plugin set `row`, holding `cur`, back to r4_md_car_lod_stock? */
static inline int r4_md_car_row_restore(R4MdOptions o, unsigned row, const int16_t cur[3])
{
    if (o.cars_full || row >= R4_MD_CAR_LOD_ROWS) return 0;
    if (o.split_same && (row == 3u || row == 4u)) return 0;   /* the plan's rows */
    return r4_md_car_row_known(row, cur) && !r4_md_car_row_eq(cur, r4_md_car_lod_stock[row]);
}

#endif /* R4_MAX_DETAIL_H */

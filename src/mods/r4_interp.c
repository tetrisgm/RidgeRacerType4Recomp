/* r4_interp.c - true in-between frames for R4 races (SLUS-00797).
 *
 * The game runs its races at 30 Hz: every other VBlank the main loop
 * (0x8001E708) runs one tick of logic, builds the next frame's ordering table,
 * waits for VSync and flips. This plugin adds images between two flips by
 * redrawing the race with the game's own draw functions while every car and
 * the camera are placed part of the way between the last two ticks.
 *
 * Where: at the entry of VSync(0) called from the main loop (0x8008B330,
 * ra 0x8001E7E4). By then the handler has produced tick n's state S_n and
 * built its OT, the previous OT has finished drawing I_{n-1} (DrawSync
 * returned), and the display is about to flip to I_{n-1}. Passes rendered
 * here show lerp(S_{n-1}, S_n, a) during I_{n-1}'s time on screen, so the
 * newest state is never shown later than stock: no added latency.
 *
 * How: each pass runs inside psx_mod_render_pass (psxrecomp render passes).
 * Guest time is frozen and RAM, CPU/GTE, GPU and VRAM are restored after it,
 * so the pass can freely overwrite car and camera values, the dead buffer's
 * ordering tables and primitive heap, and sound or RNG calls inside the draw
 * code cannot leak (SPU stores are dropped by the framework). The pass draws
 * into the buffer the game did not build this tick (bp = frame counter & 1
 * ^ 1): its RAM is dead (DrawSync returned) and its VRAM rows hold I_{n-1},
 * which the framework backs up, lets the pass draw over, captures and puts
 * back.
 *
 * What: the per-mode call lists are the race handlers' own draw paths with
 * every logic, camera, sound and timer call removed. The car/camera field
 * list and the draw-only call lists come from dogewow2048's "60 FPS" cheat
 * for the Japanese release; they are re-derived for the US EXE here and in
 * tools/data/r4_interp_fields.json.
 *
 * Gates: pacing 0x180 (races), not paused, race phase 1..3 (overlay modes),
 * the same handler before and after this tick, consecutive ticks, and the
 * framework's plan (OpenGL, flip-aware interpolation, budget). Anything else
 * shows the stock frame for that tick. Verified in runs: Grand Prix races
 * (mode 2), Time Attack (mode 1), the attract demo (mode 4) and the replay
 * after a Time Attack (mode 5); VS split screen (mode 3) is gated off until
 * a run reaches it (R4_INTERP_SPLIT_ENABLED).
 *
 * Timing: in-between frames never cost the game a frame. psxrecomp plans
 * passes only into the host time left before the game's frame is presented,
 * after the work the emulation thread still has to do for it
 * (PSX_MOD_RENDER_PASS_LEFTOVER), does not start a pass that would end after
 * that point and stops one that runs into it. The plugin runs what the plan
 * returns, middle phase first, so a plan cut short still splits the frame
 * evenly. Wherever no pass fits, or passes are unavailable (no interpolating
 * OpenGL presenter, a renderer mode that declines them, passes disabled
 * after faults), the presenter holds the game's own frame (HOLD): nothing
 * blended, nothing delayed. The log says once when passes are unavailable.
 * Frame blend is never a fallback; it is the player's Method choice
 * (r4_frame_rate_plugin.c).
 *
 * The two entry hooks belong to the manifest [[plugin]] id "r4.framerate"
 * (psx_mod_register_function_entry_plugin). psxrecomp runs a function-entry
 * hook only while the resolved mod plan activates its owner (the hook table
 * is rebuilt after the activation callbacks and cleared on initialize,
 * netplay clear and commit), so they are off while the package is disabled
 * and in netplay. They are registered once, from the package's constructor
 * (r4_interp_register_hooks); psxrecomp refuses a repeated id+address with
 * 0, so activation never registers again. */
#include "r4_interp.h"

#include <stdio.h>
#include <string.h>

#include "cpu_state.h"
#include "mod_plugins.h"
#include "r4_interp_fields.h"
#include "r4_interp_math.h"

#define R4_FR_PLUGIN "r4.framerate"

/* Main loop call sites (r4dis 0x8001E708-0x8001E844). */
#define R4_VSYNC            0x8008B330u
#define R4_CLEAR_OTAG_R     0x80093418u
#define R4_PUT_DRAW_ENV     0x80093590u
#define R4_DRAW_OTAG        0x80093520u
#define R4_RA_LOOP_HEAD     0x8001E764u   /* first ClearOTagR */
#define R4_RA_PASS_POINT    0x8001E7E4u   /* VSync(0) */
#define R4_RA_NESTED        0x8001E7E8u   /* return address for pass calls */

/* Globals (US). */
#define R4_FRAME_COUNTER    0x800AC064u   /* buffer b = counter & 1 */
#define R4_PACING           0x800AC794u   /* 0x180 = 30 Hz races */
#define R4_BUF_BASE         0x800ADCA0u
#define R4_BUF_STRIDE       0x22778u
#define R4_CUR_BUF          0x800ACDCCu
#define R4_BUF_INDEX        0x800F4DA0u
#define R4_OT2_ENABLED      0x800ACDB0u
#define R4_STATE_TABLE      0x8009EBCCu
#define R4_STATE_MAJOR      0x800F4E1Au
#define R4_STATE_MINOR      0x800F3BD6u
#define R4_PAUSED           0x800F4E18u
#define R4_PHASE            0x800FF860u
#define R4_TICK             0x800F2F94u
#define R4_DEMO_TICK        0x800ACCC4u
#define R4_DEMO_SUBMODE     0x800ACCD0u
#define R4_FADE_OUT         0x800AC788u   /* s16 */
#define R4_REPLAY_FADE      0x800AC760u
#define R4_SHOT_DEMO        0x800ACCCCu   /* TV shot counter, reset on a cut */
#define R4_SHOT_REPLAY      0x800AC824u
#define R4_CAR_LIST         0x800FFDD0u
#define R4_CAR_COUNT        0x800AC754u   /* s16 */
#define R4_SPAD_HEAP        0x1F800000u
#define R4_SPAD_OT          0x1F800004u

#define R4_MAX_PASSES 16u

/* Only a psxrecomp that plans render passes into leftover time (and stops
 * them at the deadline) keeps the game at full speed with them. Built
 * against an older one, the plugin draws no in-between frames and the game's
 * own frames are shown (tools/check_pin_keys.py stops a release first). */
#ifdef PSX_MOD_RENDER_PASS_LEFTOVER
#define R4_INTERP_RUNTIME_OK 1
#else
#define R4_INTERP_RUNTIME_OK 0
#endif

/* VS split screen (mode 3, overlay 661) has its draw sequence (seq_split)
 * from the static analysis, but no run has reached it yet: the VS battle menu
 * stays disabled without a second connected pad. Until a run verifies it
 * (render_pass_stats: no aborts, PSX_RENDER_PASS_VERIFY: 0 mismatches, pass
 * dumps), split screen shows the stock frames like menus do. */
#define R4_INTERP_SPLIT_ENABLED 0

typedef struct R4Capture {
    int32_t car[R4_INTERP_CAR_COUNT][R4_INTERP_CAR_FIELD_COUNT];
    uint8_t car_active[R4_INTERP_CAR_COUNT];
    int32_t cam[R4_INTERP_CAMERA_FIELD_COUNT];
    int32_t cam2p_ang[2][R4_INTERP_2P_FIELD_COUNT];
    int32_t cam2p_pos[2][R4_INTERP_2P_FIELD_COUNT];
    uint32_t cam2p_ang_w3[2], cam2p_pos_w3[2];   /* 4th words, copied as is */
    int32_t gte_h;
    uint32_t shot;
} R4Capture;

/* Values of the tick being interpolated: prev (n-1), cur (n), and the
 * per-group snap decisions. */
typedef struct R4Blend {
    const R4Capture *prev, *cur;
    uint8_t car_snap[R4_INTERP_CAR_COUNT];
    uint8_t cam_snap;
    uint8_t cam2p_snap[2];
    int mode;
    uint32_t bp;
    /* pre-handler values */
    int16_t pre_fade_out;
    uint32_t pre_replay_fade;
} R4Blend;

static struct {
    int enabled;            /* method = interpolate in the current session */
    int in_pass;
    /* loop head */
    int pre_valid;
    uint32_t pre_handler;
    int8_t pre_paused;
    int16_t pre_fade_out;
    uint32_t pre_replay_fade;
    /* previous tick */
    int prev_valid;
    int prev_mode;
    uint32_t prev_tick, prev_demo_tick;
    R4Capture prev, cur;
    int logged_unavailable;
    /* diagnostics */
    uint64_t ticks, interpolated, gated, passes_ok, unavailable;
    int logged_mode;
} R;

static uint32_t rd32(uint32_t a) { return psx_mod_read_word(a); }
static uint16_t rd16(uint32_t a) { return psx_mod_read_half(a); }
static void wr32(uint32_t a, uint32_t v) { psx_mod_write_word(a, v); }
static void wr16(uint32_t a, uint16_t v) { psx_mod_write_half(a, v); }

static uint32_t r4_handler(void) {
    uint32_t major = rd16(R4_STATE_MAJOR), minor = rd16(R4_STATE_MINOR);
    uint32_t row = rd32(R4_STATE_TABLE + 4u * major);
    if ((row & 0xFFE00000u) != 0x80000000u) return 0;
    return rd32(row + 4u * minor);
}

static int r4_mode_of(uint32_t handler) {
    uint32_t words[R4_SIG_WORDS];
    if ((handler & 0xFFE00003u) != 0x80000000u) return R4_MODE_NONE;
    for (unsigned i = 0; i < R4_SIG_WORDS; i++) words[i] = rd32(handler + 4u * i);
    return r4_classify_handler(handler, words);
}

/* ---- capture ---------------------------------------------------------- */

static int32_t field_read(uint32_t base, const R4InterpField *f) {
    if (f->kind == R4_FIELD_ANGLE12_H) return (int32_t)(int16_t)rd16(base + f->off);
    return (int32_t)rd32(base + f->off);
}

static void field_write(uint32_t base, const R4InterpField *f, int32_t v) {
    if (f->kind == R4_FIELD_ANGLE12_H) wr16(base + f->off, (uint16_t)v);
    else wr32(base + f->off, (uint32_t)v);
}

static void r4_capture(CPUState *cpu, R4Capture *c) {
    int16_t n = (int16_t)rd16(R4_CAR_COUNT);
    memset(c->car_active, 0, sizeof c->car_active);
    if (n < 0) n = 0;
    if (n > 16) n = 16;
    for (int i = 0; i < n; i++) {
        uint32_t p = rd32(R4_CAR_LIST + 4u * (uint32_t)i);
        for (unsigned k = 0; k < R4_INTERP_CAR_COUNT; k++)
            if (R4_INTERP_CAR_BASES[k] == p) c->car_active[k] = 1;
    }
    for (unsigned k = 0; k < R4_INTERP_CAR_COUNT; k++)
        for (unsigned f = 0; f < R4_INTERP_CAR_FIELD_COUNT; f++)
            c->car[k][f] = field_read(R4_INTERP_CAR_BASES[k], &R4_INTERP_CAR_FIELDS[f]);
    for (unsigned f = 0; f < R4_INTERP_CAMERA_FIELD_COUNT; f++)
        c->cam[f] = field_read(R4_INTERP_CAMERA_BASE, &R4_INTERP_CAMERA_FIELDS[f]);
    for (unsigned p = 0; p < 2; p++) {
        for (unsigned f = 0; f < R4_INTERP_2P_FIELD_COUNT; f++) {
            c->cam2p_ang[p][f] = field_read(R4_INTERP_2P_ANGLE_BLOCKS[p],
                                            &R4_INTERP_2P_ANGLE_FIELDS[f]);
            c->cam2p_pos[p][f] = field_read(R4_INTERP_2P_POS_BLOCKS[p],
                                            &R4_INTERP_2P_POS_FIELDS[f]);
        }
        c->cam2p_ang_w3[p] = rd32(R4_INTERP_2P_ANGLE_BLOCKS[p] + 12u);
        c->cam2p_pos_w3[p] = rd32(R4_INTERP_2P_POS_BLOCKS[p] + 12u);
    }
    c->gte_h = (int32_t)(uint16_t)cpu->gte_ctrl[26];
    c->shot = 0;
}

/* Group snaps: a car, the camera (+ zoom), or one 2P camera shows tick n
 * whole when any of its values jumped (crash, respawn, camera cut). */
static void r4_decide_snaps(R4Blend *b) {
    const R4Capture *p = b->prev, *c = b->cur;
    for (unsigned k = 0; k < R4_INTERP_CAR_COUNT; k++) {
        b->car_snap[k] = !(p->car_active[k] && c->car_active[k]);
        for (unsigned f = 0; !b->car_snap[k] && f < R4_INTERP_CAR_FIELD_COUNT; f++) {
            const R4InterpField *fd = &R4_INTERP_CAR_FIELDS[f];
            if (fd->group != R4_GROUP_BODY) continue;
            if (r4_field_snaps(fd, r4_field_delta(fd->kind, p->car[k][f], c->car[k][f])))
                b->car_snap[k] = 1;
        }
    }
    b->cam_snap = 0;
    for (unsigned f = 0; f < R4_INTERP_CAMERA_FIELD_COUNT; f++) {
        const R4InterpField *fd = &R4_INTERP_CAMERA_FIELDS[f];
        if (r4_field_snaps(fd, r4_field_delta(fd->kind, p->cam[f], c->cam[f])))
            b->cam_snap = 1;
    }
    if (b->mode == R4_MODE_DEMO || b->mode == R4_MODE_REPLAY) {
        int32_t dh = c->gte_h - p->gte_h;
        int32_t hmax = c->gte_h > p->gte_h ? c->gte_h : p->gte_h;
        if (dh < 0) dh = -dh;
        if (dh > hmax / 4 || c->shot < p->shot) b->cam_snap = 1;
    }
    for (unsigned q = 0; q < 2; q++) {
        b->cam2p_snap[q] = 0;
        for (unsigned f = 0; f < R4_INTERP_2P_FIELD_COUNT; f++) {
            const R4InterpField *fa = &R4_INTERP_2P_ANGLE_FIELDS[f];
            const R4InterpField *fp = &R4_INTERP_2P_POS_FIELDS[f];
            if (r4_field_snaps(fa, r4_field_delta(fa->kind, p->cam2p_ang[q][f], c->cam2p_ang[q][f])) ||
                r4_field_snaps(fp, r4_field_delta(fp->kind, p->cam2p_pos[q][f], c->cam2p_pos[q][f])))
                b->cam2p_snap[q] = 1;
        }
    }
}

static int32_t blend_value(const R4InterpField *f, int32_t prev, int32_t cur,
                           int group_snap, uint32_t a) {
    int32_t d = r4_field_delta(f->kind, prev, cur);
    if (group_snap || r4_field_snaps(f, d)) return cur;
    return r4_lerp(prev, d, a);
}

/* Write the in-between state into guest RAM (restored by the framework). */
static void r4_apply(const R4Blend *b, uint32_t a) {
    for (unsigned k = 0; k < R4_INTERP_CAR_COUNT; k++) {
        if (!b->cur->car_active[k] || b->car_snap[k]) continue;
        for (unsigned f = 0; f < R4_INTERP_CAR_FIELD_COUNT; f++) {
            const R4InterpField *fd = &R4_INTERP_CAR_FIELDS[f];
            field_write(R4_INTERP_CAR_BASES[k], fd,
                        blend_value(fd, b->prev->car[k][f], b->cur->car[k][f], 0, a));
        }
    }
    if (b->mode != R4_MODE_SPLIT && !b->cam_snap) {
        for (unsigned f = 0; f < R4_INTERP_CAMERA_FIELD_COUNT; f++) {
            const R4InterpField *fd = &R4_INTERP_CAMERA_FIELDS[f];
            field_write(R4_INTERP_CAMERA_BASE, fd,
                        blend_value(fd, b->prev->cam[f], b->cur->cam[f], 0, a));
        }
    }
}

static int32_t r4_blend_h(const R4Blend *b, uint32_t a) {
    if (b->cam_snap) return b->cur->gte_h;
    return r4_lerp(b->prev->gte_h, b->cur->gte_h - b->prev->gte_h, a);
}

/* ---- guest calls ------------------------------------------------------ */

typedef struct R4Call {
    CPUState *cpu;
    uint32_t sp;          /* frame for stack arguments and a text buffer */
    int broken;
} R4Call;

static void r4_arg(R4Call *c, unsigned index, uint32_t value) {
    /* index 4.. = stack arguments at sp+0x10.. */
    wr32(c->sp + 0x10u + 4u * (index - 4u), value);
}

static uint32_t r4_call4(R4Call *c, uint32_t fn, uint32_t a0, uint32_t a1,
                         uint32_t a2, uint32_t a3) {
    CPUState *cpu = c->cpu;
    if (c->broken) return 0;
    cpu->gpr[4] = a0; cpu->gpr[5] = a1; cpu->gpr[6] = a2; cpu->gpr[7] = a3;
    cpu->gpr[29] = c->sp;
    cpu->gpr[31] = R4_RA_NESTED;
    psx_dispatch_call(cpu, fn, R4_RA_NESTED);
    if (cpu->gpr[29] != c->sp) {
        /* The callee did not return through its own frame: stop drawing and
         * discard this image (the framework restores all state anyway). */
        c->broken = 1;
        return 0;
    }
    return cpu->gpr[2];
}
/* Sequences below keep their R4Call in a local named `call`. */
#define CALL0(fn)             r4_call4(&call, (fn), 0, 0, 0, 0)
#define CALL1(fn, a)          r4_call4(&call, (fn), (a), 0, 0, 0)
#define CALL2(fn, a, b)       r4_call4(&call, (fn), (a), (b), 0, 0)
#define CALL3(fn, a, b, c3)   r4_call4(&call, (fn), (a), (b), (c3), 0)
#define CALL4(fn, a, b, c3, d) r4_call4(&call, (fn), (a), (b), (c3), (d))

static uint32_t kmh_of(uint32_t car) {
    return (uint32_t)r4_kmh((int16_t)rd16(car + 0x1D8u));
}

/* ---- per-mode sequences (race draw paths without logic) -------------- */

static void seq_time_attack(R4Call *cc, const R4Blend *b, uint32_t buf) {
    R4Call call = *cc;
    uint32_t ot1 = buf + 0x70u, car0 = R4_INTERP_CAR_BASES[0];
    uint32_t tick = rd32(R4_TICK);
    (void)b;
    CALL1(0x8003C4B4u, 1);                           /* popup; a0=1: no timer/beep */
    CALL2(0x80034444u, ot1, tick);
    CALL1(0x80039C04u, car0);
    CALL1(0x8002BEA4u, 0);
    CALL2(0x800212E0u, (uint32_t)(int32_t)(int16_t)rd16(0x800F4E20u), 0);
    CALL1(0x80021134u, 0);
    CALL2(0x80021614u, kmh_of(car0), 0);
    CALL2(0x80021960u, (uint32_t)(int32_t)(int16_t)rd16(car0 + 0x27Au), 0);
    CALL1(0x80021DDCu, 0);
    CALL2(0x80022048u, ot1, 0);
    CALL1(0x80020A98u, (uint32_t)(int32_t)(int16_t)rd16(0x800F4EF4u));
    CALL0(0x8006E4D0u);                              /* camera builder */
    CALL1(0x8002E554u, 0);                           /* cars */
    if ((int16_t)rd16(0x800AC29Au) != (int16_t)rd16(0x800AD6DAu))
        CALL1(0x8002258Cu, 0);
    wr32(0x1F800078u, rd32(0x800F2C14u));
    CALL1(0x8006F00Cu, 0);
    CALL1(0x80074AD8u, 0);                           /* objects */
    CALL1(0x800374F8u, 0);                           /* course */
    *cc = call;
}

static void seq_grand_prix(R4Call *cc, const R4Blend *b, uint32_t buf) {
    R4Call call = *cc;
    uint32_t ot1 = buf + 0x70u, car0 = R4_INTERP_CAR_BASES[0];
    uint32_t tick = rd32(R4_TICK);
    (void)b;
    CALL1(0x8006F2B0u, 0);                           /* viewport 0 (handler top) */
    CALL2(0x80034444u, ot1, tick);
    CALL1(0x80039C04u, car0);
    CALL1(0x8002BEA4u, 0);
    CALL1(0x8002C0F8u, rd32(0x800AC75Cu));           /* time-limit clock */
    CALL2(0x800212E0u, (uint32_t)(int32_t)(int16_t)rd16(0x800F4E20u), 0);
    CALL1(0x80021134u, 0);
    CALL2(0x80021614u, kmh_of(car0), 0);
    CALL2(0x80021960u, (uint32_t)(int32_t)(int16_t)rd16(car0 + 0x27Au), 0);
    CALL2(0x80021A20u, (uint32_t)(int32_t)(int16_t)rd16(car0 + 0x1EEu), 0);
    CALL1(0x80021DDCu, 0);
    CALL2(0x80022048u, ot1, 0);
    CALL1(0x80021BA4u, 0);
    CALL1(0x80020A98u, (uint32_t)(int32_t)(int16_t)rd16(0x800F4EF4u));
    CALL0(0x8006E4D0u);
    CALL1(0x8002E554u, 0);
    if ((int16_t)rd16(car0 + 0x1EAu) != (int16_t)rd16(0x800AD6DAu))
        CALL1(0x8002258Cu, 0);
    wr32(0x1F800078u, rd32(0x800F2C14u));
    CALL1(0x8006F00Cu, 0);
    CALL1(0x80074AD8u, 0);
    CALL1(0x800374F8u, 0);
    CALL1(0x80070A58u, rd32(0x800AC068u));           /* rear-view mirror */
    *cc = call;
}

static void seq_split(R4Call *cc, const R4Blend *b, uint32_t buf, uint32_t a) {
    R4Call call = *cc;
    uint32_t ot2 = buf + 0xB70u;
    uint32_t tick = rd32(R4_TICK);
    CALL2(0x80034444u, ot2, tick);
    for (uint32_t i = 0; i < 2; i++) {
        uint32_t car = R4_INTERP_CAR_BASES[i];
        CALL2(0x80021614u, kmh_of(car), i);
        CALL2(0x80021774u, (uint32_t)(int32_t)(int16_t)rd16(0x800F4E20u + 2u * i), i);
        CALL2(0x80021960u, (uint32_t)(int32_t)(int16_t)rd16(car + 0x27Au), i);
        CALL2(0x80021A20u, (uint32_t)(int32_t)(int16_t)rd16(car + 0x1EEu), i);
        CALL2(0x8002C194u, rd32(0x800AC75Cu), i);
        CALL2(0x80021C38u, i + 1u, (uint32_t)(int32_t)(int16_t)rd16(car + 0x2AAu));
        CALL1(0x80021DDCu, i + 1u);
        CALL2(0x80022048u, ot2, i + 1u);
        CALL1(0x80021BA4u, i + 1u);
    }
    CALL1(0x80022340u, 0);
    CALL1(0x80021134u, 2);
    CALL1(0x80020A98u, 2);
    for (uint32_t i = 0; i < 2; i++) {
        uint32_t car = R4_INTERP_CAR_BASES[i];
        /* The per-player camera, interpolated straight into scratch. */
        for (unsigned f = 0; f < R4_INTERP_2P_FIELD_COUNT; f++) {
            const R4InterpField *fa = &R4_INTERP_2P_ANGLE_FIELDS[f];
            const R4InterpField *fp = &R4_INTERP_2P_POS_FIELDS[f];
            wr32(0x1F800018u + 4u * f,
                 (uint32_t)blend_value(fa, b->prev->cam2p_ang[i][f],
                                       b->cur->cam2p_ang[i][f], b->cam2p_snap[i], a));
            wr32(0x1F800008u + 4u * f,
                 (uint32_t)blend_value(fp, b->prev->cam2p_pos[i][f],
                                       b->cur->cam2p_pos[i][f], b->cam2p_snap[i], a));
        }
        wr32(0x1F800024u, b->cur->cam2p_ang_w3[i]);
        wr32(0x1F800014u, b->cur->cam2p_pos_w3[i]);
        CALL1(0x8006F2B0u, i + 2u);                  /* VP2 top / VP3 bottom */
        CALL0(0x8006E4D0u);
        CALL1(0x8002E554u, i + 2u);
        if ((int16_t)rd16(car + 0x1EAu) != 0) CALL1(0x8002258Cu, i + 1u);
        wr16(0x1F80005Eu, 1);
        CALL1(0x8006F00Cu, i);
        CALL1(0x80074AD8u, 0);
        CALL1(0x800374F8u, i + 1u);
    }
    *cc = call;
}

static void seq_demo(R4Call *cc, const R4Blend *b, uint32_t buf, uint32_t a) {
    R4Call call = *cc;
    uint32_t ot1 = buf + 0x70u;
    uint32_t sub = rd32(R4_DEMO_SUBMODE), demo_tick = rd32(R4_DEMO_TICK);
    int16_t v = b->pre_fade_out;
    CALL1(0x8006F2B0u, 0);
    CALL1(0x800914B0u, (uint32_t)r4_blend_h(b, a));   /* SetGeomScreen: TV zoom */
    CALL0(0x8006E4D0u);
    if ((int32_t)demo_tick >= 5) {
        if (sub == 0) CALL0(0x800559BCu); else CALL0(0x80055968u);
    }
    if (v > 0) {
        int16_t v1 = (int16_t)(v - 1);
        CALL3(0x800207D4u, ot1, (uint32_t)((0x40 - v1) * 4), 0x45u);
    } else if ((int32_t)demo_tick < 0x1F) {
        CALL3(0x800207D4u, ot1, (uint32_t)((0x1E - (int32_t)demo_tick) * 8), 0x45u);
    }
    if (sub == 1 && rd32(0x800ACD40u) == 0) {
        uint32_t track = rd32(0x800F780Cu);
        uint32_t name = rd32(0x800A0E54u + 4u * (track - 1u));
        uint32_t text = call.sp + 0x40u;              /* sprintf buffer */
        if (rd32(0x800ACCE0u)) {
            r4_arg(&call, 4, 0);
            CALL4(0x8004CBACu, 0x131u, 0xC8u, 0x800112C4u, 0xFFu);   /* "REPEAT" */
        }
        r4_arg(&call, 4, 0);
        CALL4(0x8004CBACu, 0xFAu, 0xD4u, 0x800A0B14u + 32u * name, 0xFFu);
        CALL4(0x80059E24u, 0x800A1C24u, 0xFCu, 0xD4u, 0xFFu);
        CALL4(0x80059E24u, 0x800A1C20u, 0x105u, 0xD4u, 0xFFu);
        CALL3(0x80096660u, text, 0x800112CCu, track);                 /* sprintf "%02d" */
        r4_arg(&call, 4, 0);
        CALL4(0x8004C8BCu, 0x112u, 0xD4u, text, 0xFFu);
        CALL4(0x80059E24u, 0x800A1C18u, 0x121u, 0xD4u, 0xFFu);
        CALL4(0x80059E24u, 0x800A1C1Cu, 0x12Au, 0xD4u, 0xFFu);
        CALL1(0x8004CC40u, 0);
    }
    CALL1(0x80014A84u, 10);
    if (sub == 0 || rd32(0x800ACD38u) == 0) {
        CALL1(0x8002E554u, 6);
    } else {
        for (uint32_t i = 1; i < 7; i++)
            CALL2(0x8002DC00u, rd32(R4_CAR_LIST + 4u * i), 2);
    }
    wr32(0x1F800078u, rd32(0x800F2C14u));
    CALL1(0x8006F00Cu, 0);
    CALL1(0x80074AD8u, 0);
    CALL1(0x800374F8u, 0);
    CALL0(0x800558E8u);
    *cc = call;
}

static void seq_replay(R4Call *cc, const R4Blend *b, uint32_t buf, uint32_t a) {
    R4Call call = *cc;
    uint32_t ot1 = buf + 0x70u;
    uint32_t f = b->pre_replay_fade;
    int16_t w = b->pre_fade_out;
    if ((int32_t)f < 0x1F)
        CALL3(0x800207D4u, ot1, (uint32_t)((0x1E - (int32_t)f) * 10), 0x25u);
    else
        CALL0(0x800559BCu);
    if (w >= 0) {
        int16_t w1 = (int16_t)(w - 1);
        if (w1 < 0x1E)
            CALL3(0x800207D4u, ot1, (uint32_t)((0x1E - w1) * 10), 0x45u);
    }
    if ((int32_t)CALL1(0x80090600u, (f << 7) & 0xF80u) > 0) {       /* rsin blink */
        r4_arg(&call, 4, 0x4C); r4_arg(&call, 5, 0x0C);
        r4_arg(&call, 6, 0x64); r4_arg(&call, 7, 0xC0);
        r4_arg(&call, 8, 5);    r4_arg(&call, 9, 0x7FF8);
        r4_arg(&call, 10, 0);   r4_arg(&call, 11, 0x80);
        wr32(R4_SPAD_HEAP, CALL4(0x80020074u, ot1, rd32(R4_SPAD_HEAP), 0x10u, 0x10u));
    }
    CALL1(0x8006F2B0u, 0);
    CALL1(0x800914B0u, (uint32_t)r4_blend_h(b, a));
    CALL0(0x8006E4D0u);
    CALL1(0x8002E554u, 6);
    wr32(0x1F800078u, rd32(0x800F2C14u));
    CALL1(0x8006F00Cu, 0);
    CALL1(0x80074AD8u, 0);
    CALL1(0x800374F8u, 0);
    if ((int32_t)rd32(R4_REPLAY_FADE) >= 0x1E) CALL0(0x800558E8u);
    *cc = call;
}

/* ---- the pass --------------------------------------------------------- */

static int r4_pass(struct CPUState *cpu, void *user, uint32_t alpha_q16) {
    const R4Blend *b = (const R4Blend *)user;
    uint32_t buf = R4_BUF_BASE + b->bp * R4_BUF_STRIDE;
    R4Call call;
    call.cpu = cpu;
    call.sp = (cpu->gpr[29] - 0x100u) & ~7u;
    call.broken = 0;
    R.in_pass = 1;

    /* Select the dead buffer everywhere the draw code looks for it. */
    wr32(R4_CUR_BUF, buf);
    wr32(R4_BUF_INDEX, b->bp);
    wr32(R4_SPAD_OT, buf + 0x70u);
    wr32(R4_SPAD_HEAP, buf + 0x1670u);
    CALL2(R4_CLEAR_OTAG_R, buf + 0x70u, 0x2C0u);
    CALL2(R4_CLEAR_OTAG_R, buf + 0xB70u, 0x2C0u);
    r4_apply(b, alpha_q16);

    switch (b->mode) {
    case R4_MODE_TIME_ATTACK: seq_time_attack(&call, b, buf); break;
    case R4_MODE_GRAND_PRIX:  seq_grand_prix(&call, b, buf); break;
    case R4_MODE_SPLIT:       seq_split(&call, b, buf, alpha_q16); break;
    case R4_MODE_DEMO:        seq_demo(&call, b, buf, alpha_q16); break;
    case R4_MODE_REPLAY:      seq_replay(&call, b, buf, alpha_q16); break;
    default: call.broken = 1; break;
    }

    CALL1(R4_PUT_DRAW_ENV, buf);
    CALL1(R4_DRAW_OTAG, buf + 0xB6Cu);
    if (rd32(R4_OT2_ENABLED)) CALL1(R4_DRAW_OTAG, buf + 0x166Cu);
    R.in_pass = 0;
    return !call.broken;
}

/* ---- when passes cannot run ------------------------------------------- */

static const char *r4_unavailable_reason(uint32_t status) {
    switch (status) {
    case PSX_MOD_RENDER_PASS_NO_PRESENTER:
        return "no interpolating OpenGL presenter";
    case PSX_MOD_RENDER_PASS_BACKEND:
        return "the renderer declines them in its current mode";
    case PSX_MOD_RENDER_PASS_DISABLED:
        return "disabled after repeated faults";
    default:
        return "unavailable";
    }
}

/* A race tick got no plan. Budget (nothing fits the time left) and transient
 * refusals (fast-forward, netplay, a busy machine) are routine: the game's
 * frame is shown. A lasting reason is logged once. */
static void r4_note_no_plan(void) {
    uint32_t status = psx_mod_render_pass_status();
    if (status != PSX_MOD_RENDER_PASS_NO_PRESENTER &&
        status != PSX_MOD_RENDER_PASS_BACKEND &&
        status != PSX_MOD_RENDER_PASS_DISABLED)
        return;
    R.unavailable++;
    if (!R.logged_unavailable) {
        R.logged_unavailable = 1;
        fprintf(stdout, "r4: in-between frames unavailable (%s); showing the "
                "game's own frames\n", r4_unavailable_reason(status));
    }
}

/* Order in which to run n planned phases (ascending in alphas[]): the middle
 * one first, then the middles of each half, and so on. A plan cut short at
 * its deadline then still splits the frame as evenly as it can. */
static uint32_t r4_pass_order(uint32_t n, uint32_t *order) {
    /* Each interval taken out puts two back: at most 2n + 1 ever queued. */
    uint32_t lo[2u * R4_MAX_PASSES + 2u], hi[2u * R4_MAX_PASSES + 2u];
    uint32_t head = 0, tail = 0, k = 0;
    if (n == 0) return 0;
    if (n > R4_MAX_PASSES) n = R4_MAX_PASSES;
    lo[tail] = 0; hi[tail] = n; tail++;
    while (head < tail && k < n) {
        uint32_t a = lo[head], b = hi[head], m;
        head++;
        if (a >= b) continue;
        m = a + (b - a - 1u) / 2u;
        order[k++] = m;
        lo[tail] = a; hi[tail] = m; tail++;
        lo[tail] = m + 1u; hi[tail] = b; tail++;
    }
    return k;
}

/* ---- hooks ------------------------------------------------------------ */

static void r4_loop_head(CPUState *cpu, uint32_t address) {
    (void)address;
    if (!R.enabled || R.in_pass || cpu->gpr[31] != R4_RA_LOOP_HEAD) return;
    R.pre_valid = 1;
    R.pre_handler = r4_handler();
    R.pre_paused = (int8_t)psx_mod_read_byte(R4_PAUSED);
    R.pre_fade_out = (int16_t)rd16(R4_FADE_OUT);
    R.pre_replay_fade = rd32(R4_REPLAY_FADE);
}

static int r4_gates(int mode) {
    uint32_t phase;
    if (mode == R4_MODE_NONE) return 0;
    if (mode == R4_MODE_SPLIT && !R4_INTERP_SPLIT_ENABLED) return 0;
    if (rd32(R4_PACING) != 0x180u) return 0;
    if (R.pre_paused != 0 || (int8_t)psx_mod_read_byte(R4_PAUSED) != 0) return 0;
    phase = rd32(R4_PHASE);
    if ((mode == R4_MODE_TIME_ATTACK || mode == R4_MODE_GRAND_PRIX ||
         mode == R4_MODE_SPLIT) &&
        phase - 1u >= 3u)
        return 0;
    if (mode == R4_MODE_DEMO && rd32(R4_DEMO_SUBMODE) >= 2u) return 0;
    return 1;
}

static void r4_pass_point(CPUState *cpu, uint32_t address) {
    uint32_t handler, tick, demo_tick, alphas[R4_MAX_PASSES], order[R4_MAX_PASSES];
    uint32_t n, k;
    int mode, continuous, pre_valid = R.pre_valid;
    R4Blend blend;
    (void)address;
    if (!R.enabled || R.in_pass || cpu->gpr[31] != R4_RA_PASS_POINT ||
        cpu->gpr[4] != 0u)
        return;
    R.pre_valid = 0;
    R.ticks++;
    if (!pre_valid) { R.prev_valid = 0; return; }

    handler = r4_handler();
    mode = handler == R.pre_handler ? r4_mode_of(handler) : R4_MODE_NONE;
    if (!r4_gates(mode)) {
        /* Not a race tick: the game's own frames. */
        R.prev_valid = 0;
        R.gated++;
        return;
    }

    tick = rd32(R4_TICK);
    demo_tick = rd32(R4_DEMO_TICK);
    r4_capture(cpu, &R.cur);
    R.cur.shot = mode == R4_MODE_DEMO ? rd32(R4_SHOT_DEMO)
               : mode == R4_MODE_REPLAY ? rd32(R4_SHOT_REPLAY) : 0u;
    continuous = R.prev_valid && R.prev_mode == mode &&
                 (mode == R4_MODE_DEMO ? demo_tick == R.prev_demo_tick + 1u
                                       : tick == R.prev_tick + 1u);
    n = 0;
    if (continuous) {
        /* Ask the presenter which phases of the coming frame it will show,
         * as many as fit the time left: the frame flips at the next VBlank
         * and is first presented one VBlank later, for two VBlanks (0x180
         * pacing). */
        n = psx_mod_render_pass_plan(2u, 1u, alphas, R4_MAX_PASSES);
        if (!n) r4_note_no_plan();
    }
    if (n) {
        uint64_t kept = R.passes_ok;
        uint32_t b = rd32(R4_FRAME_COUNTER) & 1u;
        uint32_t disp = R4_BUF_BASE + b * R4_BUF_STRIDE + 0x5Cu;
        PSXModRenderPass pass;
        memset(&blend, 0, sizeof blend);
        blend.prev = &R.prev;
        blend.cur = &R.cur;
        blend.mode = mode;
        blend.bp = b ^ 1u;
        blend.pre_fade_out = R.pre_fade_out;
        blend.pre_replay_fade = R.pre_replay_fade;
        r4_decide_snaps(&blend);
        memset(&pass, 0, sizeof pass);
        pass.struct_size = sizeof pass;
        /* The rect the flip shows: DISPENV of the buffer built this tick. */
        pass.x = rd16(disp);
        pass.y = rd16(disp + 2u);
        pass.w = rd16(disp + 4u);
        pass.h = rd16(disp + 6u);
        k = r4_pass_order(n, order);
        for (uint32_t i = 0; i < k; i++) {
            pass.alpha_q16 = alphas[order[i]];
            if (psx_mod_render_pass(cpu, &pass, r4_pass, &blend))
                R.passes_ok++;
            /* r4_pass clears this itself only when it returns; a pass the
             * framework stops (watchdog, deadline) leaves by longjmp. */
            R.in_pass = 0;
        }
        R.interpolated++;
        if (R.passes_ok != kept && R.logged_mode != mode) {
            static const char *const names[] = {
                "", "Time Attack", "Grand Prix", "VS split screen",
                "attract demo", "replay"};
            R.logged_mode = mode;
            fprintf(stdout, "r4: interpolating race mode %d, %s "
                    "(%u passes/frame)\n", mode, names[mode], (unsigned)n);
        }
    }
    R.prev = R.cur;
    R.prev_valid = 1;
    R.prev_mode = mode;
    R.prev_tick = tick;
    R.prev_demo_tick = demo_tick;
}

/* Entry hooks accepted by psxrecomp (2 = both); set once per process. */
static int s_hooks_registered;
static int s_hooks_tried, s_logged_unregistered;

int r4_interp_register_hooks(void) {
    if (!s_hooks_tried) {
        s_hooks_tried = 1;
        s_hooks_registered =
            psx_mod_register_function_entry_plugin(R4_FR_PLUGIN, R4_VSYNC,
                                                   r4_pass_point) +
            psx_mod_register_function_entry_plugin(R4_FR_PLUGIN,
                                                   R4_CLEAR_OTAG_R,
                                                   r4_loop_head);
    }
    return s_hooks_registered;
}

void r4_interp_activate(int enabled) {
    R.enabled = enabled ? 1 : 0;
    R.in_pass = 0;
    R.prev_valid = 0;
    R.pre_valid = 0;
    R.logged_mode = 0;
    R.logged_unavailable = 0;
    /* The hooks were registered from the constructor and fire only while
     * this plugin is activated; they still check R.enabled, since a session
     * may pick Frame blend. Without them nothing draws in-between frames:
     * the presenter holds the game's own frames (HOLD), as with no pass. */
    if (enabled && s_hooks_registered != 2) {
        R.enabled = 0;
        if (!s_logged_unregistered) {
            s_logged_unregistered = 1;
            fprintf(stderr, "r4: frame-rate hooks not registered (%d of 2); "
                    "showing the game's own frames\n", s_hooks_registered);
        }
    }
    if (R.enabled && !R4_INTERP_RUNTIME_OK) {
        static int logged;
        R.enabled = 0;
        if (!logged) {
            logged = 1;
            fprintf(stderr, "r4: this psxrecomp may draw in-between frames in "
                    "time the game needs (no PSX_MOD_RENDER_PASS_LEFTOVER); "
                    "showing the game's own frames\n");
        }
    }
}

/* r4_max_detail_plugin.c - r4.enhancement.max-detail ("R4 Max Detail").
 *
 * Lifts R4's distance-based detail reducers (r4_max_detail.h,
 * docs/MAX_DETAIL.md):
 *
 *   - course detail and split screen: two scratch values the course
 *     renderer reads, written at its entry (0x80060F94) every time it runs;
 *   - draw distance: psxrecomp's [[draw_distance.clamp]] switch (game.toml
 *     lists the course renderers' 18 ordering-table guards) and, at Maximum,
 *     the neighbouring track sections' blocks appended to the frame's course
 *     list (r4_pvs.h, shared with the widescreen plugin);
 *   - car detail: guarded [[patch]] writes in the package manifest; with Car
 *     detail = Stock a VBlank callback puts back the stock rows a save state
 *     made with Always full carried in;
 *   - car reflections: at the env-map car draw's entry (0x80014A90), in a
 *     live race drawn without a widescreen margin, the game's "off" page (-1)
 *     becomes the page race init and replays use (10); a view that turns wide
 *     gets the -1 back;
 *   - mirror scenery (off by default): the rear-view mirror's course list
 *     count is read before the mirror's block limit lowers it (0x80071704)
 *     and put back before the list is used (0x8006EDEC).
 *
 * Everything is inert unless the package is enabled: the entry hooks run
 * only while the resolved mod plan activates this plugin (psxrecomp rebuilds
 * the hook table after activation and clears it on commit and netplay
 * clear), activation is the only thing that sets s_enabled, and the clamp
 * switch is reset to off at every session start. Netplay clears the plan,
 * so a netplay match always plays stock.
 */
#include "mod_plugins.h"
#include "cpu_state.h"
#include "psx_cycles.h"
#include "r4_max_detail.h"
#include "r4_pvs.h"
#include "r4_widescreen_scene.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PKG "r4.enhancement.max-detail"
#define FEATURE "max-detail"
#define PLUGIN_ID "r4.maxdetail"

/* Registered once from the constructor (psxrecomp refuses a repeated
 * id+address with 0); activation reports a shortfall. */
static int s_hooks_registered;
#define R4_MD_REGISTER_ENTRY(address, callback) \
    (s_hooks_registered += \
         psx_mod_register_function_entry_plugin(PLUGIN_ID, (address), (callback)))
#define R4_MD_HOOK_COUNT 13

/* ---- guest layout (US) ------------------------------------------------- */
#define R4_DRAW_OTAG_FN     0x80093520u   /* trace only */
#define R4_HEAP_PTR_ADDR    0x1F800000u
#define R4_BUF0             0x800ADCA0u
#define R4_BUF_STRIDE       0x22778u
#define R4_BUF_OT1          0x70u         /* ClearOTagR(buf+0x70, 0x2C0) */
#define R4_BUF_OT1_HEAD     0xB6Cu
#define R4_BUF_OT2_HEAD     0x166Cu
#define R4_OT_ENTRIES       0x2C0u
#define R4_BUF_HEAP         0x1670u
#define R4_BUF_HEAP_END     0x22688u

static int s_enabled;
static int s_trace;
static R4MdOptions s_opt;
static int s_clamp_available;
static int s_car_restore_logged;
static int s_sections_override = -1;   /* R4_MD_SECTIONS: developer A/B */
static int s_ahead_override = -1, s_behind_override = -1;   /* R4_MD_AHEAD/BEHIND */
static int s_far_xform_off;                                    /* R4_MD_FAR_XFORM=0 */
static struct {
    int valid;
    uint32_t section, octant, yaw;
} s_pvs;
/* Maximum's budget governor (r4_md_gov_update). */
static R4MdGovernor s_gov;
static struct {
    int started, measured, last_oct;
    uint64_t start;
} s_frame;
static struct {
    int valid;
    uint32_t out;           /* a0 of 0x8006F160: SVECTOR, then VECTOR, MATRIX */
    int32_t d[3];           /* the true object - camera delta */
} s_xf;
static struct {
    int valid;
    uint32_t built;   /* the mirror list's count before the limit */
} s_mirror;
static int s_env_wrote;   /* the reflection page holds this plugin's write */
/* R4_MD_TRACE=2: pop-in census. The main view's final list of the previous
 * frame; a block that joins it and has a vertex in view is a pop, and its
 * nearest in-view vertex distance (world >> 8) is recorded. */
static struct {
    uint32_t prev[R4_PVS_LIST_MAX], nprev;
    uint32_t pops, pop_min, hist[6];
} s_pop;
static struct {
    uint32_t frames, course_calls, heap_max, heap_limit_hits;
    uint32_t ot1_max, ot2_max, pvs_frames, pvs_before_max, pvs_after_max, pvs_added;
    uint32_t nsec;
    uint32_t env_calls, env_writes, env_offs, mirror_lists, mirror_now_max, mirror_after_max;
    uint32_t xf_calls, xf_fixed, xf_culled, dir_fwd, dir_back, car_far;
    uint32_t gov_frames, busy_max_pm, level_min, level_max;
    int32_t level_sum;
} s_stat;

static uint32_t rd32(uint32_t a) { return psx_mod_read_word(a); }
static uint16_t rd16(uint32_t a) { return psx_mod_read_half(a); }

/* ---- course renderer entry --------------------------------------------- */

/* 0x80060F94(0x1F800000, list, type): the main, alternate and mirror course
 * draws all come through here, after the game has written both scratch
 * values for this draw. */
static void r4_md_course(CPUState *cpu, uint32_t address)
{
    (void)cpu;
    (void)address;
    if (!s_enabled) return;
    if (s_opt.course_full)
        psx_mod_write_half(R4_MD_COURSE_FAR_ADDR, (uint16_t)R4_MD_COURSE_FAR_FULL);
    if (s_opt.split_same)
        psx_mod_write_half(R4_MD_COURSE_2P_ADDR, 0u);
    if (s_trace) s_stat.course_calls++;
}

/* ---- course visibility (Maximum) ----------------------------------------- */

static void r4_md_pvs_octant(CPUState *cpu, uint32_t address)
{
    (void)address;
    if (!s_enabled || cpu->gpr[31] != R4_PVS_OCTANT_RA) return;
    /* 0x8006F5AC: s0 = section (set in this call's delay slot), octant from
     * the camera yaw exactly as this function computes it. */
    s_pvs.section = cpu->gpr[16];
    s_pvs.yaw = rd32(cpu->gpr[4] + 0x14u);
    s_pvs.octant = r4_pvs_octant(s_pvs.yaw);
    s_pvs.valid = 1;
}

/* Block bounds, cached per block address and checked against the block's
 * kind words (a new course or a save state rebuilds them). */
#define R4_MD_BOUNDS_CACHE 4096u
static struct {
    uint32_t block;
    R4MdBounds b;
} s_bounds[R4_MD_BOUNDS_CACHE];
static struct {
    double camx, camz, fx, fz, slope;
    uint32_t kept, dropped;
} s_cone;

static int r4_md_keep_block(void *ctx, uint32_t block)
{
    (void)ctx;
    uint32_t h = ((block >> 4) * 2654435761u) >> 20;   /* 12 bits */
    for (uint32_t probe = 0; probe < 8u; probe++) {
        uint32_t i = (h + probe) & (R4_MD_BOUNDS_CACHE - 1u);
        if (s_bounds[i].block == block) {
            if (s_bounds[i].b.sig != r4_md_block_sig(rd32, block) &&
                !r4_md_block_bounds(rd32, block, &s_bounds[i].b))
                return 1;
            int v = r4_md_block_visible(&s_bounds[i].b, s_cone.camx, s_cone.camz, s_cone.fx,
                                        s_cone.fz, s_cone.slope);
            if (v) s_cone.kept++; else s_cone.dropped++;
            return v;
        }
        if (s_bounds[i].block == 0u) {
            if (!r4_md_block_bounds(rd32, block, &s_bounds[i].b)) return 1;
            s_bounds[i].block = block;
            int v = r4_md_block_visible(&s_bounds[i].b, s_cone.camx, s_cone.camz, s_cone.fx,
                                        s_cone.fz, s_cone.slope);
            if (v) s_cone.kept++; else s_cone.dropped++;
            return v;
        }
    }
    R4MdBounds b;   /* cache full here: compute without keeping */
    if (!r4_md_block_bounds(rd32, block, &b)) return 1;
    return r4_md_block_visible(&b, s_cone.camx, s_cone.camz, s_cone.fx, s_cone.fz, s_cone.slope);
}

static int s_frame_has_course;   /* a main course draw ran this frame */
static void r4_md_pvs_merge_body(void);
static void r4_md_pop_census(uint32_t yaw);
static void r4_md_pvs_merge(CPUState *cpu, uint32_t address)
{
    (void)address;
    if (!s_enabled || !s_pvs.valid) return;
    s_pvs.valid = 0;
    uint32_t ra = cpu->gpr[31];
    if (ra != R4_PVS_MERGE_RA1 && ra != R4_PVS_MERGE_RA2) return;
    s_frame_has_course = 1;
    r4_md_pvs_merge_body();
    if (s_trace >= 2) r4_md_pop_census(s_pvs.yaw);
}

static void r4_md_pvs_merge_body(void)
{
    /* With the widescreen package live this also covers the wide octants of
     * the neighbouring sections; its own hook adds the camera section's. */
    int oct_reach = r4_pvs_octant_reach(psx_mod_widescreen_x_margin());
    if (oct_reach != s_frame.last_oct) {   /* the view changed width */
        s_frame.last_oct = oct_reach;
        r4_md_gov_reset(&s_gov, r4_md_gov_start_level(oct_reach));
    }
    int ahead = 0, behind = 0;
    if (s_opt.draw == R4_MD_DRAW_MAXIMUM) {
        ahead = r4_md_level_ahead(s_gov.level);
        behind = r4_md_level_behind(s_gov.level);
    }
    if (s_sections_override >= 0) ahead = behind = s_sections_override;
    if (s_ahead_override >= 0) ahead = s_ahead_override;
    if (s_behind_override >= 0) behind = s_behind_override;
    if (ahead == 0 && behind == 0) return;
    uint32_t nsec = r4_pvs_section_count(rd32);
    int dir = r4_pvs_ahead_dir(rd32, s_pvs.section, nsec, s_pvs.yaw);
    if (dir == 0) {   /* no centreline: both ways, at most two sections */
        int both = ahead < 2 ? ahead : 2;
        if (behind > both) both = behind;
        ahead = behind = both;
        dir = -1;
    }
    if (s_trace) {
        if (dir < 0) s_stat.dir_fwd++;
        else s_stat.dir_back++;
    }
    double t = (double)(s_pvs.yaw & 0xFFFu) * (6.283185307179586 / 4096.0);
    s_cone.fx = sin(t);
    s_cone.fz = cos(t);
    s_cone.camx = (double)(int32_t)rd32(R4_MD_CAMERA_POS_ADDR) / 4.0;
    s_cone.camz = (double)(int32_t)rd32(R4_MD_CAMERA_POS_ADDR + 8u) / 4.0;
    s_cone.slope = r4_md_view_slope(psx_mod_widescreen_x_margin());
    R4PvsMerge m = r4_pvs_merge_dir(rd32, rd16, psx_mod_write_word, s_pvs.section,
                                    s_pvs.octant, nsec, dir, ahead, behind, oct_reach,
                                    r4_md_keep_block, 0);
    if (s_trace && m.ok) {
        s_stat.nsec = nsec;
        s_stat.pvs_frames++;
        if (m.before > s_stat.pvs_before_max) s_stat.pvs_before_max = m.before;
        if (m.after > s_stat.pvs_after_max) s_stat.pvs_after_max = m.after;
        s_stat.pvs_added += m.after - m.before;
    }
}

/* ---- far object transform -------------------------------------------------- */

/* 0x8006F160(out, pos, matrix) places a car, trackside object or effect: it
 * stores pos - camera as an SVECTOR (16-bit, `sh`), rotates it by the camera
 * matrix with the GTE (0x800910A0: MVMVA, 32-bit MAC result) and loads the
 * result as the translation (0x80091320). Only the 16-bit delta limits the
 * range: past 32767 (world >> 6) per axis it wraps and the object is drawn
 * huge and misplaced, which is why the car cull distance stays 8704. When
 * the true delta does not fit, the plugin recomputes the translation from
 * the 32-bit delta exactly as MVMVA would (sf = 1) at SetTransMatrix's entry.
 * In range nothing is written, so the stock draw distances are unchanged. */
static void r4_md_xf_entry(CPUState *cpu, uint32_t address)
{
    (void)address;
    s_xf.valid = 0;
    if (!s_enabled || s_far_xform_off || !r4_md_far_xform_on(s_opt)) return;
    uint32_t pos = cpu->gpr[5];
    int32_t d[3];
    for (unsigned i = 0; i < 3u; i++)
        d[i] = (int32_t)(rd32(pos + 4u * i) - rd32(R4_MD_CAMERA_POS_ADDR + 4u * i));
    if (s_trace) s_stat.xf_calls++;
    if (r4_md_delta_fits(d)) return;
    s_xf.valid = 1;
    s_xf.out = cpu->gpr[4];
    memcpy(s_xf.d, d, sizeof d);
}

static void r4_md_xf_set_trans(CPUState *cpu, uint32_t address)
{
    (void)address;
    if (!s_xf.valid) return;
    s_xf.valid = 0;
    if (!s_enabled || cpu->gpr[31] != R4_MD_XF_SETTRANS_RA ||
        cpu->gpr[4] != s_xf.out + R4_MD_XF_MATRIX_OFF)
        return;
    int16_t m[9];
    for (unsigned i = 0; i < 9u; i++)
        m[i] = (int16_t)rd16(R4_MD_CAMERA_MATRIX_ADDR + 2u * i);
    int32_t t[3];
    r4_md_apply_matrix(m, s_xf.d, t);
    if (!r4_md_xf_safe(t)) {
        t[0] = t[1] = 0;
        t[2] = R4_MD_XF_BEHIND;
        if (s_trace) s_stat.xf_culled++;
    }
    for (unsigned i = 0; i < 3u; i++) {
        psx_mod_write_word(s_xf.out + R4_MD_XF_VECTOR_OFF + 4u * i, (uint32_t)t[i]);
        psx_mod_write_word(s_xf.out + R4_MD_XF_MATRIX_OFF + 0x14u + 4u * i, (uint32_t)t[i]);
    }
    if (s_trace) s_stat.xf_fixed++;
}

/* ---- pop-in census (R4_MD_TRACE=2) -------------------------------------------- */

/* Nearest vertex of `block` inside a +/-`k256`/256 slope view cone around
 * the camera's heading (fx, fz), in world >> 8 units, or 0 when no sampled
 * vertex is in view. Course vertices are s16 with world = v + 0x8000 (x, z);
 * the camera is at 0x1F800008 in world >> 6. Samples vertex 0 of up to 16
 * polys of each kind. */
static uint32_t r4_md_block_view_dist(uint32_t block, double fx, double fz, int k256)
{
    double cx = (double)(int32_t)rd32(R4_MD_CAMERA_POS_ADDR) / 4.0;
    double cz = (double)(int32_t)rd32(R4_MD_CAMERA_POS_ADDR + 8u) / 4.0;
    uint32_t best = 0u;
    for (unsigned k = 0; k < R4_MD_POLY_KINDS; k++) {
        uint32_t w = rd32(block + r4_md_poly_kinds[k].field);
        uint32_t polys = rd32(r4_md_poly_kinds[k].table);
        uint32_t n = w & 0xFFFFu, first = w >> 16;
        if ((polys & 0xFFE00000u) != 0x80000000u || n == 0u || n > 4096u) continue;
        uint32_t step = n > 16u ? n / 16u : 1u;
        for (uint32_t i = 0; i < n; i += step) {
            uint32_t poly = polys + r4_md_poly_kinds[k].stride * (first + i);
            uint32_t xy = rd32(poly), zz = rd32(poly + 4u);
            double x = (double)(int16_t)(xy & 0xFFFFu) + 32768.0 - cx;
            double z = (double)(int16_t)(zz & 0xFFFFu) + 32768.0 - cz;
            double fwd = x * fx + z * fz, lat = x * fz - z * fx;
            if (fwd <= 0.0 || (lat < 0 ? -lat : lat) * 256.0 > fwd * k256) continue;
            uint32_t d = (uint32_t)sqrt(x * x + z * z) + 1u;
            if (!best || d < best) best = d;
        }
    }
    return best;
}

static void r4_md_pop_census(uint32_t yaw)
{
    uint32_t n = rd32(R4_PVS_LIST_ADDR);
    if (n > R4_PVS_LIST_MAX) return;
    double t = (double)(yaw & 0xFFFu) * (6.283185307179586 / 4096.0);
    double fx = sin(t), fz = cos(t);
    int margin = psx_mod_widescreen_x_margin();
    int k256 = 320 + (margin > 0 ? margin * 2 : 0);   /* 4:3 about +/-51 deg, wider with margin */
    uint32_t cur[R4_PVS_LIST_MAX];
    for (uint32_t i = 0; i < n; i++) cur[i] = rd32(R4_PVS_LIST_ADDR + 4u + 4u * i);
    if (s_pop.nprev) {
        for (uint32_t i = 0; i < n; i++) {
            uint32_t j = 0;
            while (j < s_pop.nprev && s_pop.prev[j] != cur[i]) j++;
            if (j < s_pop.nprev) continue;
            uint32_t d = r4_md_block_view_dist(cur[i], fx, fz, k256);
            if (!d) continue;
            s_pop.pops++;
            if (!s_pop.pop_min || d < s_pop.pop_min) s_pop.pop_min = d;
            s_pop.hist[d < 4000u ? 0 : d < 8000u ? 1 : d < 12000u ? 2 : d < 16000u ? 3 : d < 24000u ? 4 : 5]++;
        }
    }
    memcpy(s_pop.prev, cur, n * sizeof cur[0]);
    s_pop.nprev = n;
}

/* ---- far cars ------------------------------------------------------------------ */

static struct {
    int valid;
    uint32_t addr;
    int16_t saved[3];
} s_car;

static void r4_md_car_restore(void)
{
    if (!s_car.valid) return;
    for (unsigned i = 0; i < 3u; i++)
        psx_mod_write_half(s_car.addr + 2u * i, (uint16_t)s_car.saved[i]);
    s_car.valid = 0;
}

/* Far cars at this moment: Extended always (no sections to pay for), Maximum
 * from governor level 1 (R4_MD_FAR_CARS=0/1 overrides for A/B runs). */
static int s_far_cars_override = -1;
static int r4_md_far_cars_now(void)
{
    if (s_far_cars_override >= 0) return s_far_cars_override;
    if (s_opt.draw == R4_MD_DRAW_EXTENDED) return 1;
    return s_opt.draw == R4_MD_DRAW_MAXIMUM && r4_md_level_far_cars(s_gov.level);
}

/* 0x8002DC00(car, row): raise the row for this lookup (r4_md_car_row_far). */
static void r4_md_car_draw(CPUState *cpu, uint32_t address)
{
    (void)address;
    r4_md_car_restore();
    if (!s_enabled || s_far_xform_off) return;
    uint32_t row = cpu->gpr[5];
    if (row >= R4_MD_CAR_LOD_ROWS) return;
    uint32_t a = R4_MD_CAR_LOD_TABLE + 6u * row;
    int16_t cur[3] = { (int16_t)rd16(a), (int16_t)rd16(a + 2u), (int16_t)rd16(a + 4u) }, out[3];
    if (!r4_md_far_cars_now()) return;
    if (!r4_md_car_row_far(s_opt, row, cur, out)) return;
    s_car.valid = 1;
    s_car.addr = a;
    memcpy(s_car.saved, cur, sizeof cur);
    for (unsigned i = 0; i < 3u; i++)
        if (out[i] != cur[i]) psx_mod_write_half(a + 2u * i, (uint16_t)out[i]);
}

/* 0x80015F60 from 0x8002DC00, after its last row read: put the row back. */
static void r4_md_car_after_lod(CPUState *cpu, uint32_t address)
{
    (void)address;
    if (!s_car.valid) return;
    uint32_t ra = cpu->gpr[31];
    for (unsigned i = 0; i < 4u; i++) {
        if (ra != r4_md_car_after_lod_ra[i]) continue;
        if (s_trace && i == 2u) s_stat.car_far++;
        r4_md_car_restore();
        return;
    }
}

/* ---- frame budget (Maximum) --------------------------------------------------- */

/* 0x8009375C from the main loop, right after VSync(0): a frame starts. */
static void r4_md_frame_start(CPUState *cpu, uint32_t address)
{
    (void)address;
    if (!s_enabled || cpu->gpr[31] != R4_MD_FRAME_START_RA) return;
    s_frame.start = psx_cycle_count;
    s_frame.started = 1;
    s_frame.measured = 0;
}

/* VSync from the main loop's floor wait: the frame's work (handler, DrawSync)
 * is done. The first call of the frame ends the measurement. */
static void r4_md_vsync(CPUState *cpu, uint32_t address)
{
    (void)address;
    if (!s_enabled || !s_frame.started || s_frame.measured ||
        cpu->gpr[31] != R4_MD_VSYNC_WAIT_RA)
        return;
    s_frame.measured = 1;
    if (s_opt.draw != R4_MD_DRAW_MAXIMUM || !s_frame_has_course) return;
    s_frame_has_course = 0;
    uint64_t busy = psx_cycle_count - s_frame.start;
    (void)r4_md_gov_update(&s_gov, busy);
    if (s_trace) {
        uint32_t pm = (uint32_t)(busy * 1000u / R4_MD_FRAME_BUDGET);
        if (pm > s_stat.busy_max_pm) s_stat.busy_max_pm = pm;
        if (!s_stat.gov_frames || s_gov.level < (int)s_stat.level_min) s_stat.level_min = (uint32_t)(s_gov.level + 100);
        if (!s_stat.gov_frames || s_gov.level + 100 > (int)s_stat.level_max) s_stat.level_max = (uint32_t)(s_gov.level + 100);
        s_stat.level_sum += s_gov.level;
        s_stat.gov_frames++;
    }
}

/* A save state's frames run from the starting level again. */
static void r4_md_savestate_loaded(void)
{
    if (!s_enabled) return;
    r4_md_gov_reset(&s_gov, r4_md_gov_start_level(s_frame.last_oct));
    memset(&s_frame, 0, sizeof s_frame);
    s_frame.last_oct = -1;
    s_car.valid = 0;
    s_xf.valid = 0;
}

/* ---- car reflections --------------------------------------------------------- */

/* 0x80014A90 draws one car body part with the environment map, or returns 0
 * at once when the page word is negative (the caller then draws the plain
 * mesh). Both of its reads of the page follow this entry with no store in
 * between, so the part is drawn with the page written here. */
static void r4_md_env(CPUState *cpu, uint32_t address)
{
    (void)cpu;
    (void)address;
    if (!s_enabled || !s_opt.reflections) return;
    if (s_trace) s_stat.env_calls++;
    uint32_t page = rd32(R4_MD_ENV_TPAGE_ADDR);
    if ((int32_t)page >= 0 && !s_env_wrote) return;   /* the game's own page */
    int live = rd32(R4_RACE_PHASE_ADDR) >= 1u && r4_in_race(rd32, rd16);
    if (!live || (int32_t)page < 0) s_env_wrote = 0;  /* the game set it since */
    int wide = psx_mod_widescreen_x_margin() > 0;
    int act = r4_md_reflections_action(s_opt, live, wide, page, s_env_wrote);
    if (act == R4_MD_ENV_WRITE_ON) {
        psx_mod_write_word(R4_MD_ENV_TPAGE_ADDR, R4_MD_ENV_TPAGE_RACE);
        s_env_wrote = 1;
        if (s_trace) s_stat.env_writes++;
    } else if (act == R4_MD_ENV_WRITE_OFF) {
        psx_mod_write_word(R4_MD_ENV_TPAGE_ADDR, 0xFFFFFFFFu);
        s_env_wrote = 0;
        if (s_trace) s_stat.env_offs++;
    }
}

/* ---- mirror scenery --------------------------------------------------------- */

/* 0x80071704 (the mirror's block limit), called by the mirror draw right
 * after it built the list: remember the full count. */
static void r4_md_mirror_limit(CPUState *cpu, uint32_t address)
{
    (void)address;
    s_mirror.valid = 0;
    if (!s_enabled || !s_opt.mirror_full || cpu->gpr[31] != R4_MD_MIRROR_LIMIT_RA) return;
    s_mirror.built = rd32(R4_MD_LIST_ADDR);
    s_mirror.valid = 1;
}

/* 0x8006EDEC (the list's first consumer), from the mirror draw: put the
 * count back. The limit wrote only the count word, so the block pointers
 * past it are still the ones 0x8006EB58 built. */
static void r4_md_mirror_list(CPUState *cpu, uint32_t address)
{
    (void)address;
    if (!s_enabled || !s_mirror.valid) return;
    s_mirror.valid = 0;
    if (cpu->gpr[31] != R4_MD_MIRROR_LIST_RA) return;
    uint32_t now = rd32(R4_MD_LIST_ADDR);
    uint32_t want = r4_md_mirror_count(s_opt, s_mirror.built, now);
    if (want != now) psx_mod_write_word(R4_MD_LIST_ADDR, want);
    if (s_trace) {
        s_stat.mirror_lists++;
        if (now > s_stat.mirror_now_max) s_stat.mirror_now_max = now;
        if (want > s_stat.mirror_after_max) s_stat.mirror_after_max = want;
    }
}

/* ---- car table (Car detail = Stock) ---------------------------------------- */

/* A save state made with Car detail = Always full keeps the full rows; with
 * this package on and Car detail = Stock, put the stock rows back at the next
 * VBlank (r4_md_car_row_restore). Only after the EXE entry, where psxrecomp
 * applies (and guard-checks) the plan's own writes. Reads only, in a session
 * that never loaded such a state. With the package off nothing runs, so the
 * state's table stays until the game reloads its EXE (docs/MAX_DETAIL.md). */
static void r4_md_vblank(void)
{
    if (!s_enabled || s_opt.cars_full || !psx_mod_game_started()) return;
    int restored = 0;
    for (unsigned r = 0; r < R4_MD_CAR_LOD_ROWS; r++) {
        uint32_t a = R4_MD_CAR_LOD_TABLE + 6u * r;
        int16_t cur[3] = { (int16_t)rd16(a), (int16_t)rd16(a + 2u), (int16_t)rd16(a + 4u) };
        if (!r4_md_car_row_restore(s_opt, r, cur)) continue;
        for (unsigned i = 0; i < 3u; i++)
            psx_mod_write_half(a + 2u * i, (uint16_t)r4_md_car_lod_stock[r][i]);
        restored = 1;
    }
    if (restored && !s_car_restore_logged) {
        s_car_restore_logged = 1;
        fprintf(stdout, "[r4-md] car table set back to Car detail = Stock "
                        "(a save state made with Always full)\n");
        fflush(stdout);
    }
}

/* ---- diagnostics (R4_MD_TRACE=1) ----------------------------------------- */

/* Highest occupied slot of a ClearOTagR table ending at `head`, below the
 * backdrop slots (701/702) and the viewport's DR_AREA (703). An empty slot
 * links to the one before it. */
static uint32_t r4_md_ot_high(uint32_t head)
{
    uint32_t first = head - 4u * (R4_OT_ENTRIES - 1u);
    for (uint32_t i = R4_OT_ENTRIES - 4u; i > 0u; i--) {
        uint32_t link = rd32(first + 4u * i) & 0x00FFFFFFu;
        if (link != ((first + 4u * (i - 1u)) & 0x00FFFFFFu)) return i;
    }
    return 0u;
}

static void r4_md_draw_otag(CPUState *cpu, uint32_t address)
{
    (void)address;
    if (!s_enabled || !s_trace) return;
    uint32_t a0 = cpu->gpr[4];
    int ot1 = 1;
    uint32_t buf = a0 - R4_BUF_OT1_HEAD;
    if (buf != R4_BUF0 && buf != R4_BUF0 + R4_BUF_STRIDE) {
        buf = a0 - R4_BUF_OT2_HEAD;
        ot1 = 0;
        if (buf != R4_BUF0 && buf != R4_BUF0 + R4_BUF_STRIDE) return;
    }
    uint32_t hi = r4_md_ot_high(a0);
    if (ot1) {
        if (hi > s_stat.ot1_max) s_stat.ot1_max = hi;
    } else if (hi > s_stat.ot2_max) {
        s_stat.ot2_max = hi;
    }
    if (!ot1) return;
    uint32_t heap = rd32(R4_HEAP_PTR_ADDR);
    uint32_t used = heap - (buf + R4_BUF_HEAP);
    if (heap >= buf + R4_BUF_HEAP && used <= R4_BUF_STRIDE) {
        if (used > s_stat.heap_max) s_stat.heap_max = used;
        if (used >= R4_BUF_HEAP_END - R4_BUF_HEAP) s_stat.heap_limit_hits++;
    }
    if (++s_stat.frames % 60u == 0u) {
        fprintf(stdout,
                "[r4-md] heap_max=0x%X/0x%X (%u%%) limit_hits=%u ot1_max=%u ot2_max=%u "
                "course_calls=%u pvs sections=%u before_max=%u after_max=%u added=%u/%u frames "
                "clamp=%d env=%u/%u mirror=%u->%u (%u lists) env_off=%u "
                "xf=%u/%u/%u carfar=%u level=%d..%d avg=%d busy_max=%u dir=%u/%u cone=%u/%u pops=%u min=%u h=%u/%u/%u/%u/%u/%u\n",
                (unsigned)s_stat.heap_max, (unsigned)(R4_BUF_HEAP_END - R4_BUF_HEAP),
                (unsigned)(100u * s_stat.heap_max / (R4_BUF_HEAP_END - R4_BUF_HEAP)),
                (unsigned)s_stat.heap_limit_hits, (unsigned)s_stat.ot1_max,
                (unsigned)s_stat.ot2_max, (unsigned)s_stat.course_calls,
                (unsigned)s_stat.nsec, (unsigned)s_stat.pvs_before_max,
                (unsigned)s_stat.pvs_after_max, (unsigned)s_stat.pvs_added,
                (unsigned)s_stat.pvs_frames, psx_mod_draw_distance_clamp_enabled(),
                (unsigned)s_stat.env_writes, (unsigned)s_stat.env_calls,
                (unsigned)s_stat.mirror_now_max, (unsigned)s_stat.mirror_after_max,
                (unsigned)s_stat.mirror_lists, (unsigned)s_stat.env_offs,
                (unsigned)s_stat.xf_fixed, (unsigned)s_stat.xf_calls,
                (unsigned)s_stat.xf_culled, (unsigned)s_stat.car_far,
                (int)s_stat.level_min - 100, (int)s_stat.level_max - 100,
                s_stat.gov_frames ? (int)(s_stat.level_sum / (int32_t)s_stat.gov_frames) : 0,
                (unsigned)s_stat.busy_max_pm,
                (unsigned)s_stat.dir_fwd, (unsigned)s_stat.dir_back,
                (unsigned)s_cone.kept, (unsigned)(s_cone.kept + s_cone.dropped),
                (unsigned)s_pop.pops, (unsigned)s_pop.pop_min, (unsigned)s_pop.hist[0],
                (unsigned)s_pop.hist[1], (unsigned)s_pop.hist[2], (unsigned)s_pop.hist[3],
                (unsigned)s_pop.hist[4], (unsigned)s_pop.hist[5]);
        fflush(stdout);
        s_stat.heap_max = s_stat.ot1_max = s_stat.ot2_max = s_stat.course_calls = 0;
        s_stat.pvs_before_max = s_stat.pvs_after_max = 0;
        s_stat.pvs_added = s_stat.pvs_frames = 0;
        s_stat.env_calls = s_stat.env_writes = s_stat.env_offs = 0;
        s_stat.mirror_lists = s_stat.mirror_now_max = s_stat.mirror_after_max = 0;
        s_stat.xf_calls = s_stat.xf_fixed = s_stat.dir_fwd = s_stat.dir_back = 0;
        s_stat.xf_culled = s_stat.car_far = 0;
        s_stat.gov_frames = s_stat.busy_max_pm = s_stat.level_min = s_stat.level_max = 0;
        s_stat.level_sum = 0;
        s_pop.pops = s_pop.pop_min = 0;
        s_cone.kept = s_cone.dropped = 0;
        memset(s_pop.hist, 0, sizeof s_pop.hist);
    }
}

/* ---- activation ------------------------------------------------------------ */

static void r4_md_option(const char *id, char *out, size_t cap)
{
    if (!psx_mod_option_value(PKG, FEATURE, id, out, cap)) out[0] = '\0';
}

static void r4_max_detail_activate(void)
{
    char draw[16], course[16], cars[16], split[16], reflections[16], mirror[16];
    const char *env = getenv("R4_MD_TRACE");
    s_trace = env ? atoi(env) : 0;
    memset(&s_pop, 0, sizeof s_pop);
    env = getenv("R4_MD_SECTIONS");
    s_sections_override = env ? atoi(env) : -1;
    env = getenv("R4_MD_AHEAD");
    s_ahead_override = env ? atoi(env) : -1;
    env = getenv("R4_MD_BEHIND");
    s_behind_override = env ? atoi(env) : -1;
    memset(&s_car, 0, sizeof s_car);
    memset(&s_frame, 0, sizeof s_frame);
    s_frame.last_oct = -1;
    s_frame_has_course = 0;
    r4_md_gov_reset(&s_gov, 0);
    env = getenv("R4_MD_FAR_CARS");
    s_far_cars_override = env ? atoi(env) : -1;
    env = getenv("R4_MD_FAR_XFORM");
    s_far_xform_off = env && env[0] == '0';
    memset(s_bounds, 0, sizeof s_bounds);
    memset(&s_cone, 0, sizeof s_cone);

    memset(&s_xf, 0, sizeof s_xf);
    memset(&s_pvs, 0, sizeof s_pvs);
    memset(&s_mirror, 0, sizeof s_mirror);
    s_env_wrote = 0;
    memset(&s_stat, 0, sizeof s_stat);
    s_car_restore_logged = 0;
    r4_md_option("draw_distance", draw, sizeof draw);
    r4_md_option("course", course, sizeof course);
    r4_md_option("cars", cars, sizeof cars);
    r4_md_option("split_screen", split, sizeof split);
    r4_md_option("reflections", reflections, sizeof reflections);
    r4_md_option("mirror", mirror, sizeof mirror);
    s_opt = r4_md_options(draw, course, cars, split, reflections, mirror);
    s_clamp_available = psx_mod_set_draw_distance_clamp(r4_md_clamp_on(s_opt));
    s_enabled = 1;
    if (r4_md_clamp_on(s_opt) && !s_clamp_available)
        fprintf(stderr, "[r4-md] game.toml lists no [[draw_distance.clamp]] sites; "
                        "draw distance stays stock\n");
    if (s_hooks_registered != R4_MD_HOOK_COUNT)
        fprintf(stderr, "[r4-md] only %d of %d entry hooks registered\n",
                s_hooks_registered, R4_MD_HOOK_COUNT);
    fprintf(stdout, "[r4-md] active: draw=%d course=%d cars=%d split=%d reflections=%d "
                    "mirror=%d clamp=%d\n",
            (int)s_opt.draw, s_opt.course_full, s_opt.cars_full, s_opt.split_same,
            s_opt.reflections, s_opt.mirror_full, psx_mod_draw_distance_clamp_enabled());
    fflush(stdout);
}

PSX_MOD_CONSTRUCTOR(r4_register_max_detail)
{
    R4_MD_REGISTER_ENTRY(R4_MD_COURSE_DISPATCH_FN, r4_md_course);
    R4_MD_REGISTER_ENTRY(R4_PVS_OCTANT_FN, r4_md_pvs_octant);
    R4_MD_REGISTER_ENTRY(R4_PVS_MERGE_FN, r4_md_pvs_merge);
    R4_MD_REGISTER_ENTRY(R4_DRAW_OTAG_FN, r4_md_draw_otag);
    R4_MD_REGISTER_ENTRY(R4_MD_ENV_RENDER_FN, r4_md_env);
    R4_MD_REGISTER_ENTRY(R4_MD_MIRROR_LIMIT_FN, r4_md_mirror_limit);
    R4_MD_REGISTER_ENTRY(R4_MD_MIRROR_LIST_FN, r4_md_mirror_list);
    R4_MD_REGISTER_ENTRY(R4_MD_XF_FN, r4_md_xf_entry);
    R4_MD_REGISTER_ENTRY(R4_MD_XF_SETTRANS_FN, r4_md_xf_set_trans);
    R4_MD_REGISTER_ENTRY(R4_MD_CAR_DRAW_FN, r4_md_car_draw);
    R4_MD_REGISTER_ENTRY(R4_MD_CAR_AFTER_LOD_FN, r4_md_car_after_lod);
    R4_MD_REGISTER_ENTRY(R4_MD_FRAME_START_FN, r4_md_frame_start);
    R4_MD_REGISTER_ENTRY(R4_MD_VSYNC_FN, r4_md_vsync);
    (void)psx_mod_register_savestate_plugin(PLUGIN_ID, r4_md_savestate_loaded);
    (void)psx_mod_register_activation_plugin(PLUGIN_ID, r4_max_detail_activate);
    (void)psx_mod_register_vblank_plugin(PLUGIN_ID, r4_md_vblank);
}

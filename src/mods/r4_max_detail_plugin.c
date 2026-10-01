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
 *     made with Always full carried in.
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
#include "r4_max_detail.h"
#include "r4_pvs.h"

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
#define R4_MD_HOOK_COUNT 4

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
static struct {
    int valid;
    uint32_t section, octant;
} s_pvs;
static struct {
    uint32_t frames, course_calls, heap_max, heap_limit_hits;
    uint32_t ot1_max, ot2_max, pvs_frames, pvs_before_max, pvs_after_max, pvs_added;
    uint32_t nsec;
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
    s_pvs.octant = r4_pvs_octant(rd32(cpu->gpr[4] + 0x14u));
    s_pvs.valid = 1;
}

static void r4_md_pvs_merge(CPUState *cpu, uint32_t address)
{
    (void)address;
    if (!s_enabled || !s_pvs.valid) return;
    s_pvs.valid = 0;
    uint32_t ra = cpu->gpr[31];
    if (ra != R4_PVS_MERGE_RA1 && ra != R4_PVS_MERGE_RA2) return;
    /* With the widescreen package live this also covers the wide octants of
     * the neighbouring sections; its own hook adds the camera section's. */
    int oct_reach = r4_pvs_octant_reach(psx_mod_widescreen_x_margin());
    int sec_reach = s_sections_override >= 0
                        ? s_sections_override
                        : r4_md_section_reach_for(s_opt, oct_reach);
    if (sec_reach == 0) return;
    uint32_t nsec = r4_pvs_section_count(rd32);
    R4PvsMerge m = r4_pvs_merge(rd32, rd16, psx_mod_write_word, s_pvs.section,
                                s_pvs.octant, nsec, sec_reach, oct_reach);
    if (s_trace && m.ok) {
        s_stat.nsec = nsec;
        s_stat.pvs_frames++;
        if (m.before > s_stat.pvs_before_max) s_stat.pvs_before_max = m.before;
        if (m.after > s_stat.pvs_after_max) s_stat.pvs_after_max = m.after;
        s_stat.pvs_added += m.after - m.before;
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
                "clamp=%d\n",
                (unsigned)s_stat.heap_max, (unsigned)(R4_BUF_HEAP_END - R4_BUF_HEAP),
                (unsigned)(100u * s_stat.heap_max / (R4_BUF_HEAP_END - R4_BUF_HEAP)),
                (unsigned)s_stat.heap_limit_hits, (unsigned)s_stat.ot1_max,
                (unsigned)s_stat.ot2_max, (unsigned)s_stat.course_calls,
                (unsigned)s_stat.nsec, (unsigned)s_stat.pvs_before_max,
                (unsigned)s_stat.pvs_after_max, (unsigned)s_stat.pvs_added,
                (unsigned)s_stat.pvs_frames, psx_mod_draw_distance_clamp_enabled());
        fflush(stdout);
        s_stat.heap_max = s_stat.ot1_max = s_stat.ot2_max = s_stat.course_calls = 0;
        s_stat.pvs_before_max = s_stat.pvs_after_max = 0;
        s_stat.pvs_added = s_stat.pvs_frames = 0;
    }
}

/* ---- activation ------------------------------------------------------------ */

static void r4_md_option(const char *id, char *out, size_t cap)
{
    if (!psx_mod_option_value(PKG, FEATURE, id, out, cap)) out[0] = '\0';
}

static void r4_max_detail_activate(void)
{
    char draw[16], course[16], cars[16], split[16];
    const char *env = getenv("R4_MD_TRACE");
    s_trace = env && env[0] == '1';
    env = getenv("R4_MD_SECTIONS");
    s_sections_override = env ? atoi(env) : -1;
    memset(&s_pvs, 0, sizeof s_pvs);
    memset(&s_stat, 0, sizeof s_stat);
    s_car_restore_logged = 0;
    r4_md_option("draw_distance", draw, sizeof draw);
    r4_md_option("course", course, sizeof course);
    r4_md_option("cars", cars, sizeof cars);
    r4_md_option("split_screen", split, sizeof split);
    s_opt = r4_md_options(draw, course, cars, split);
    s_clamp_available = psx_mod_set_draw_distance_clamp(r4_md_clamp_on(s_opt));
    s_enabled = 1;
    if (r4_md_clamp_on(s_opt) && !s_clamp_available)
        fprintf(stderr, "[r4-md] game.toml lists no [[draw_distance.clamp]] sites; "
                        "draw distance stays stock\n");
    if (s_hooks_registered != R4_MD_HOOK_COUNT)
        fprintf(stderr, "[r4-md] only %d of %d entry hooks registered\n",
                s_hooks_registered, R4_MD_HOOK_COUNT);
    fprintf(stdout, "[r4-md] active: draw=%d course=%d cars=%d split=%d clamp=%d\n",
            (int)s_opt.draw, s_opt.course_full, s_opt.cars_full, s_opt.split_same,
            psx_mod_draw_distance_clamp_enabled());
    fflush(stdout);
}

PSX_MOD_CONSTRUCTOR(r4_register_max_detail)
{
    R4_MD_REGISTER_ENTRY(R4_MD_COURSE_DISPATCH_FN, r4_md_course);
    R4_MD_REGISTER_ENTRY(R4_PVS_OCTANT_FN, r4_md_pvs_octant);
    R4_MD_REGISTER_ENTRY(R4_PVS_MERGE_FN, r4_md_pvs_merge);
    R4_MD_REGISTER_ENTRY(R4_DRAW_OTAG_FN, r4_md_draw_otag);
    (void)psx_mod_register_activation_plugin(PLUGIN_ID, r4_max_detail_activate);
    (void)psx_mod_register_vblank_plugin(PLUGIN_ID, r4_md_vblank);
}

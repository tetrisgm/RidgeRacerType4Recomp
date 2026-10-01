/* r4_widescreen_plugin.c - r4.enhancement.widescreen ("R4 Custom Renderer").
 *
 * Native-wide widescreen for races. psxrecomp's native-wide renderer keeps
 * R4's projection and draws the extra columns into a wider surface; this
 * plugin supplies the R4-specific parts:
 *
 *   - activation: the View option (Fit to Window, 16:9, 21:9, 32:9);
 *   - a world-scene predicate so only races widen (menus, results, the garage
 *     and movies stay 4:3, pillarboxed);
 *   - HUD anchoring: the 18 HUD producers' packets are tagged left/right so
 *     the compositor moves them to the true window edges;
 *   - course visibility: neighbouring heading octants' blocks are added to the
 *     frame's course list so the wide edges do not lose road and scenery.
 *
 * The screen-X culls themselves are widened by game.toml [widescreen.cull]
 * (tools/r4_ws_scan.py). No guest code is patched; the only guest-RAM write
 * is the per-frame course list, which the game rebuilds every frame.
 *
 * Everything is inert unless the package is enabled: the entry hooks run only
 * while the mod plan activates this plugin, activation is the only thing that
 * sets s_enabled, and every callback checks it first.
 *
 * Pattern after mstan's TombaRecomp / MegaManX6Recomp custom renderers
 * (written fresh against mod_plugins.h). HUD producer classes and the race
 * predicate derive from dogewow2048's R4 JP patch analysis; see the headers.
 */
#include "mod_plugins.h"
#include "cpu_state.h"
#include "r4_widescreen_hud.h"
#include "r4_pvs.h"
#include "r4_widescreen_scene.h"
#include "r4_widescreen_view.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PKG "r4.enhancement.widescreen"
#define FEATURE "widescreen"
#define PLUGIN_ID "r4.widescreen"

/* The entry hooks belong to this package's manifest [[plugin]] id. psxrecomp
 * runs a function-entry hook only while the resolved mod plan activates its
 * owner (it rebuilds the hook table after the activation callbacks and clears
 * it on initialize, netplay clear and commit), so they never fire while the
 * package is disabled or in netplay. s_enabled (set only by activation) stays
 * as a second guard. They are registered once, from the constructor; psxrecomp
 * refuses a repeated id+address with 0, so the accepted ones are counted and
 * activation reports a shortfall. */
static int s_hooks_registered;
#define R4_WS_REGISTER_ENTRY(address, callback) \
    (s_hooks_registered += \
         psx_mod_register_function_entry_plugin(PLUGIN_ID, (address), (callback)))

/* ---- guest layout (US) ------------------------------------------------- */
#define R4_HEAP_PTR_ADDR    0x1F800000u   /* next free primitive-heap byte */
#define R4_CUR_BUF_ADDR     0x800ACDCCu   /* current frame buffer base */
#define R4_BUF0             0x800ADCA0u
#define R4_BUF_STRIDE       0x22778u
#define R4_BUF_OT1          0x70u         /* ClearOTagR(buf+0x70, 0x2C0) */
#define R4_BUF_OT1_HEAD     0xB6Cu        /* DrawOTag(buf+0xB6C) */
#define R4_BUF_OT2_HEAD     0x166Cu       /* DrawOTag(buf+0x166C) */
#define R4_BUF_HEAP         0x1670u
#define R4_BUF_HEAP_END     0x22688u      /* heap: 0x21018 bytes */
#define R4_BUF_TAIL_END     0x22778u      /* persistent HUD packets */
#define R4_PVS_OCTANT_FN    0x8006F584u   /* octant(a0 = camera) */
#define R4_PVS_OCTANT_RA    0x8006F5D8u   /* ... called by the list lookup */
#define R4_PVS_MERGE_FN     0x8007166Cu   /* first consumer of the list */
#define R4_PVS_MERGE_RA1    0x8006F02Cu   /* course draw, path 1 */
#define R4_PVS_MERGE_RA2    0x8006F090u   /* course draw, path 2 */
#define R4_CLEAR_OTAG_FN    0x80093418u
#define R4_DRAW_OTAG_FN     0x80093520u

#ifndef R4_WS_PVS_UNION
#define R4_WS_PVS_UNION 1
#endif

/* A HUD producer's range ends at the next producer or at one of these calls,
 * whichever comes first in every race draw path (overlays 659/660/661 and the
 * demo and after-goal handlers). */
static const uint32_t r4_hud_closers[] = {
    0x80020A98u, 0x80021A20u, 0x80021BA4u, 0x80022340u, 0x80034B50u,
    0x800207D4u, 0x8006E4D0u, 0x8006F2B0u, 0x8002E554u, 0x8006F00Cu,
    0x80074AD8u, 0x800374F8u, 0x80070A58u,
};
#define R4_WS_CLOSER_COUNT (sizeof r4_hud_closers / sizeof r4_hud_closers[0])
/* game.toml mod_function_entry_funcs: producers, closers, ClearOTagR,
 * DrawOTag and the two course-list hooks. */
#define R4_WS_HOOK_COUNT (R4_HUD_PRODUCER_COUNT + R4_WS_CLOSER_COUNT + 4)

typedef struct {
    uint32_t start, end;
    uint8_t cls;
} R4HudRange;

#define R4_WS_RANGES_MAX 64
#define R4_WS_KEYS_MAX   2048
typedef struct {
    R4HudRange ranges[R4_WS_RANGES_MAX];
    int nranges;
    uint32_t keys[R4_WS_KEYS_MAX];   /* tagged this frame: cleared on reuse */
    int nkeys;
} R4BufferTags;

static int s_enabled;
static int s_pvs_union = R4_WS_PVS_UNION;
static int s_trace;
static R4BufferTags s_buf[2];
static struct {
    int open, buf;
    uint32_t start;
    uint8_t cls;
} s_open;
static struct {
    int valid;
    uint32_t section, octant;
} s_pvs;
static struct {
    uint32_t frames, heap_max, heap_limit_hits, tags, ranges, bad_ranges;
    uint32_t pvs_frames, pvs_before_max, pvs_after_max, pvs_added;
    uint32_t last_handler, last_phase;
    int last_race;
} s_stat;

static uint32_t rd32(uint32_t a) { return psx_mod_read_word(a); }
static uint16_t rd16(uint32_t a) { return psx_mod_read_half(a); }

static int r4_buffer_index(uint32_t buf)
{
    if (buf == R4_BUF0) return 0;
    if (buf == R4_BUF0 + R4_BUF_STRIDE) return 1;
    return -1;
}
static uint32_t r4_buffer_base(int b) { return R4_BUF0 + (uint32_t)b * R4_BUF_STRIDE; }

static int r4_race_scene(void)
{
    return s_enabled && r4_in_race(rd32, rd16);
}

/* Callbacks do work only in a widened race. */
static int r4_ws_live(void)
{
    return s_enabled && psx_mod_widescreen_x_margin() > 0 && r4_in_race(rd32, rd16);
}

/* ---- HUD ----------------------------------------------------------------- */

static void r4_hud_close(void)
{
    if (!s_open.open) return;
    s_open.open = 0;
    R4BufferTags *bt = &s_buf[s_open.buf];
    uint32_t base = r4_buffer_base(s_open.buf);
    uint32_t lo = base + R4_BUF_HEAP, hi = base + R4_BUF_HEAP_END;
    uint32_t end = rd32(R4_HEAP_PTR_ADDR);
    if (s_open.start < lo || end > hi || end < s_open.start) {
        s_stat.bad_ranges++;
        return;
    }
    if (end == s_open.start || bt->nranges >= R4_WS_RANGES_MAX) return;
    bt->ranges[bt->nranges].start = s_open.start;
    bt->ranges[bt->nranges].end = end;
    bt->ranges[bt->nranges].cls = s_open.cls;
    bt->nranges++;
}

static void r4_hud_enter(CPUState *cpu, uint32_t address)
{
    (void)cpu;
    if (!s_enabled) return;
    r4_hud_close();
    if (!r4_ws_live()) return;
    int cls = r4_hud_class_of(address);
    if (cls <= R4_HUD_CENTRE) return;        /* centred HUD stays untagged */
    int b = r4_buffer_index(rd32(R4_CUR_BUF_ADDR));
    if (b < 0) return;
    s_open.open = 1;
    s_open.buf = b;
    s_open.start = rd32(R4_HEAP_PTR_ADDR);
    s_open.cls = (uint8_t)cls;
}

static void r4_hud_closer(CPUState *cpu, uint32_t address)
{
    (void)cpu;
    (void)address;
    if (s_enabled) r4_hud_close();
}

static void r4_hud_tag(void *ctx, uint32_t key, int edge)
{
    R4BufferTags *bt = (R4BufferTags *)ctx;
    psx_mod_tag_hud_primitive(key, edge);
    if (bt->nkeys < R4_WS_KEYS_MAX) bt->keys[bt->nkeys++] = key;
    s_stat.tags++;
}

static void r4_ws_trace_frame(uint32_t buf);

/* DrawOTag(ot head): the frame's packets are complete. Tag the recorded HUD
 * ranges and the persistent HUD packets now, so the tags are fresh while the
 * GPU consumes this ordering table (roles expire after two VBlanks). */
static void r4_draw_otag(CPUState *cpu, uint32_t address)
{
    (void)address;
    if (!s_enabled) return;
    r4_hud_close();
    uint32_t a0 = cpu->gpr[4];
    int ot1 = 1, b = r4_buffer_index(a0 - R4_BUF_OT1_HEAD);
    if (b < 0) {
        b = r4_buffer_index(a0 - R4_BUF_OT2_HEAD);
        ot1 = 0;
    }
    if (b < 0) return;
    uint32_t base = r4_buffer_base(b);
    R4BufferTags *bt = &s_buf[b];
    if (s_trace && ot1) r4_ws_trace_frame(base);
    if (r4_ws_live()) {
        for (int i = 0; i < bt->nranges; i++) {
            const R4HudRange *r = &bt->ranges[i];
            if (r4_hud_walk(rd32, r->start, r->end, r->cls, r4_hud_tag, bt) < 0)
                s_stat.bad_ranges++;
            s_stat.ranges++;
        }
        if (ot1)
            (void)r4_hud_walk(rd32, base + R4_BUF_HEAP_END, base + R4_BUF_TAIL_END,
                              R4_HUD_AUTO, r4_hud_tag, bt);
    }
    bt->nranges = 0;
}

/* ClearOTagR(buf+0x70): buffer `buf` starts a new frame. Clear the tags its
 * previous frame set, so a reused packet address never keeps a HUD tag
 * (at frame rates above 30 Hz a buffer comes back within the tag lifetime).
 * Keyed by the argument, not the return address, so it also holds for
 * frames drawn outside the main loop. */
static void r4_clear_otag(CPUState *cpu, uint32_t address)
{
    (void)address;
    if (!s_enabled) return;
    int b = r4_buffer_index(cpu->gpr[4] - R4_BUF_OT1);
    if (b < 0) return;
    R4BufferTags *bt = &s_buf[b];
    for (int i = 0; i < bt->nkeys; i++) psx_mod_tag_hud_primitive(bt->keys[i], 0);
    bt->nkeys = 0;
    bt->nranges = 0;
    if (s_open.open && s_open.buf == b) s_open.open = 0;
}

/* ---- course visibility ----------------------------------------------------- */

static void r4_ws_pvs_octant(CPUState *cpu, uint32_t address)
{
    (void)address;
    if (!s_enabled || !s_pvs_union || cpu->gpr[31] != R4_PVS_OCTANT_RA) return;
    /* 0x8006F5AC: s0 = section (set in this call's delay slot), octant from
     * the camera yaw exactly as this function computes it. */
    s_pvs.section = cpu->gpr[16];
    s_pvs.octant = (((rd32(cpu->gpr[4] + 0x14u) & 0xFFFu) + 0x100u) >> 9) & 7u;
    s_pvs.valid = 1;
}

static void r4_ws_pvs_merge(CPUState *cpu, uint32_t address)
{
    (void)address;
    if (!s_enabled || !s_pvs.valid) return;
    s_pvs.valid = 0;
    uint32_t ra = cpu->gpr[31];
    if (ra != R4_PVS_MERGE_RA1 && ra != R4_PVS_MERGE_RA2) return;
    if (!r4_ws_live()) return;
    int reach = r4_pvs_octant_reach(psx_mod_widescreen_x_margin());
    /* The neighbouring octants of the camera's section (r4_pvs.h; R4 Max
     * Detail adds neighbouring sections through the same helper). */
    R4PvsMerge m = r4_pvs_merge(rd32, rd16, psx_mod_write_word, s_pvs.section,
                                s_pvs.octant, 0u, 0, reach);
    if (!m.ok) return;
    uint32_t before = m.before, count = m.after;
    if (s_trace) {
        s_stat.pvs_frames++;
        if (before > s_stat.pvs_before_max) s_stat.pvs_before_max = before;
        if (count > s_stat.pvs_after_max) s_stat.pvs_after_max = count;
        s_stat.pvs_added += count - before;
    }
}

/* ---- diagnostics (R4_WS_TRACE=1) ----------------------------------------- */

static void r4_ws_trace_frame(uint32_t buf)
{
    uint32_t heap = rd32(R4_HEAP_PTR_ADDR);
    uint32_t used = heap - (buf + R4_BUF_HEAP);
    if (heap >= buf + R4_BUF_HEAP && used <= R4_BUF_TAIL_END) {
        if (used > s_stat.heap_max) s_stat.heap_max = used;
        if (used >= R4_BUF_HEAP_END - R4_BUF_HEAP) s_stat.heap_limit_hits++;
    }
    uint32_t handler = r4_frame_handler(rd32, rd16);
    uint32_t phase = rd32(R4_RACE_PHASE_ADDR);
    int race = r4_in_race(rd32, rd16);
    if (handler != s_stat.last_handler || race != s_stat.last_race ||
        (phase >= 4u) != (s_stat.last_phase >= 4u)) {
        fprintf(stdout, "[r4-ws] scene: major=%u minor=%u handler=%08X phase=%u race=%d\n",
                (unsigned)rd16(R4_STATE_MAJOR_ADDR), (unsigned)rd16(R4_STATE_MINOR_ADDR),
                (unsigned)handler, (unsigned)phase, race);
        s_stat.last_handler = handler;
        s_stat.last_race = race;
        s_stat.last_phase = phase;
    }
    if (++s_stat.frames % 60u == 0u) {
        fprintf(stdout,
                "[r4-ws] margin=%d heap_max=0x%X/0x%X (%u%%) limit_hits=%u "
                "hud_tags=%u ranges=%u bad=%u pvs before_max=%u after_max=%u added=%u/%u frames\n",
                (int)psx_mod_widescreen_x_margin(), (unsigned)s_stat.heap_max,
                (unsigned)(R4_BUF_HEAP_END - R4_BUF_HEAP),
                (unsigned)(100u * s_stat.heap_max / (R4_BUF_HEAP_END - R4_BUF_HEAP)),
                (unsigned)s_stat.heap_limit_hits, (unsigned)s_stat.tags,
                (unsigned)s_stat.ranges, (unsigned)s_stat.bad_ranges,
                (unsigned)s_stat.pvs_before_max, (unsigned)s_stat.pvs_after_max,
                (unsigned)s_stat.pvs_added, (unsigned)s_stat.pvs_frames);
        fflush(stdout);
        s_stat.heap_max = s_stat.tags = s_stat.ranges = 0;
        s_stat.pvs_before_max = s_stat.pvs_after_max = 0;
        s_stat.pvs_added = s_stat.pvs_frames = 0;
    }
}

/* ---- activation ------------------------------------------------------------ */

static void r4_widescreen_activate(void)
{
    char choice[16];
    const char *env;
    s_enabled = 1;
    memset(s_buf, 0, sizeof s_buf);
    memset(&s_open, 0, sizeof s_open);
    memset(&s_pvs, 0, sizeof s_pvs);
    memset(&s_stat, 0, sizeof s_stat);
    env = getenv("R4_WS_TRACE");
    s_trace = env && env[0] == '1';
    env = getenv("R4_WS_PVS");          /* developer A/B: 0 off, 1 on */
    s_pvs_union = env ? env[0] == '1' : R4_WS_PVS_UNION;

    if (!psx_mod_option_value(PKG, FEATURE, "aspect", choice, sizeof choice))
        strcpy(choice, "Fit");
    R4WidescreenView view = r4_widescreen_view(choice);
    (void)psx_mod_set_fixed_display_aspect(view.numerator, view.denominator);
    if (view.fit)
        /* No upper limit (owner decision; the heap never overflows). Past
         * about 65:9 the right reveal outruns the GTE's screen-X range and
         * renders black (docs/WIDESCREEN.md, Limitations). */
        (void)psx_mod_set_adaptive_display_aspect(0u, 0u);
    psx_mod_set_world_scene_predicate(r4_race_scene);
    if (s_hooks_registered != R4_WS_HOOK_COUNT)
        fprintf(stderr, "[r4-ws] only %d of %d entry hooks registered\n",
                s_hooks_registered, (int)R4_WS_HOOK_COUNT);
    if (s_trace)
        fprintf(stdout, "[r4-ws] active: view=%s pvs_union=%d hooks=%d\n", choice,
                s_pvs_union, s_hooks_registered);
}

PSX_MOD_CONSTRUCTOR(r4_register_widescreen)
{
    for (int i = 0; i < R4_HUD_PRODUCER_COUNT; i++)
        R4_WS_REGISTER_ENTRY(r4_hud_producers[i].fn, r4_hud_enter);
    for (size_t i = 0; i < R4_WS_CLOSER_COUNT; i++)
        R4_WS_REGISTER_ENTRY(r4_hud_closers[i], r4_hud_closer);
    R4_WS_REGISTER_ENTRY(R4_CLEAR_OTAG_FN, r4_clear_otag);
    R4_WS_REGISTER_ENTRY(R4_DRAW_OTAG_FN, r4_draw_otag);
    R4_WS_REGISTER_ENTRY(R4_PVS_OCTANT_FN, r4_ws_pvs_octant);
    R4_WS_REGISTER_ENTRY(R4_PVS_MERGE_FN, r4_ws_pvs_merge);
    (void)psx_mod_register_activation_plugin(PLUGIN_ID, r4_widescreen_activate);
}

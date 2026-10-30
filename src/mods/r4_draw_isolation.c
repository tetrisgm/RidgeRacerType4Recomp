/* r4_draw_isolation.c - draw-isolation gate for R4 races (SLUS-00797).
 *
 * Developer tool (package r4.debug.draw-isolation, channel developer: never
 * ships). Before anyone moves R4's frame draw onto worker threads this proves,
 * over a whole race, that the draw calls of a race tick can run against a
 * private copy of guest RAM, scratchpad and GTE registers: their stores never
 * reach the logic, and the logic still behaves and draws exactly as stock.
 * Findings and the workflow: docs/DRAW_ISOLATION.md.
 *
 * Every top-level call of a race-handler draw function (the call lists of
 * race handler, which are the race draw paths with logic removed) and the
 * main loop's two ClearOTagR calls run nested from their entry hook inside
 * psx_draw_iso_enter()/leave() (psxrecomp draw_isolation.h), in place on the
 * guest clock. At VSync(0) (the pass point: tick built, display about to
 * flip) the frame output - DRAWENV/DISPENV and every OT entry and packet the
 * DrawOTag walk will read - is handed to the main context, and one line per
 * tick is logged:
 *
 *   seq,counter,tick,handler,mode,phase,spans,irq,mmio_w,mmio_r,owned_ram,
 *   owned_spad,owned_gte,loop_reads_owned,ot_words,ot_hash,ram_hash,
 *   spad_hash,gte_hash,ram_hash_all
 *
 * ram_hash leaves out garbage by design (the dead stack below the main loop's
 * frame and the kernel's register save of an interrupted context);
 * ram_hash_all is the whole of RAM.
 *
 * The hashes are of the merged (stock-equivalent) machine, so a stock run of
 * the same input logs the same lines; tools/draw_iso_gate.py compares them.
 *
 * Modes (option "mode", or env R4_DRAW_ISO_MODE):
 *   base   hooks log only (the reference run);
 *   nest   draw calls run nested from their hook, no isolation (checks the
 *          mechanism itself is invisible: must equal base);
 *   iso    the gate: isolation; main keeps its own value for every byte
 *          the draw wrote, as the main thread of a threaded build would;
 *   poison diagnostic: main sees draw-owned bytes inverted, so every main
 *          read of one diverges, harmless garbage reads included.
 * Env: R4_DRAW_ISO_LOG (tick log path, default draw_iso_ticks.csv),
 * R4_DRAW_ISO_SUMMARY (per-function write map, default draw_iso_summary.txt),
 * R4_DRAW_ISO_PUBLIC (lo-hi[,lo-hi..] guest ranges handed to main at every
 * leave: ablation), R4_DRAW_ISO_SKIP (comma list of function addresses left
 * on main), R4_DRAW_ISO_REGS=1 (poison caller-saved GPRs after each call).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpu_state.h"
#include "draw_isolation.h"
#include "mod_plugins.h"
#include "psx_memory.h"
#include "r4_race_mode.h"

extern int psx_get_in_exception(void);

#define ISO_PACKAGE "r4.debug.draw-isolation"
#define ISO_PLUGIN  "r4.draw-isolation"

#define R4_VSYNC            0x8008B330u
#define R4_CLEAR_OTAG_R     0x80093418u
#define R4_RA_CLEAR_OT1     0x8001E764u
#define R4_RA_CLEAR_OT2     0x8001E774u
#define R4_RA_VSYNC1        0x8001E7D0u   /* VSync(1) spin: handler done */
#define R4_RA_PASS_POINT    0x8001E7E4u   /* VSync(0) */
#define R4_FRAME_COUNTER    0x800AC064u
#define R4_PACING           0x800AC794u
#define R4_BUF_BASE         0x800ADCA0u
#define R4_BUF_STRIDE       0x22778u
#define R4_CUR_BUF          0x800ACDCCu
#define R4_OT2_ENABLED      0x800ACDB0u
#define R4_STATE_TABLE      0x8009EBCCu
#define R4_STATE_MAJOR      0x800F4E1Au
#define R4_STATE_MINOR      0x800F3BD6u
#define R4_PHASE            0x800FF860u
#define R4_TICK             0x800F2F94u

/* Draw entry points of the race handlers (race handler call lists; static
 * audit in analysis/jp-hack/impl2/fps_r4.md). All are trusted entry hooks in
 * game.toml mod_function_entry_funcs. */
static const uint32_t k_draw_fns[] = {
    /* HUD producers */
    0x8003C4B4u, 0x80034444u, 0x80039C04u, 0x8002BEA4u, 0x8002C0F8u, 0x8002C194u,
    0x800212E0u, 0x80021134u, 0x80021614u, 0x80021774u, 0x80021960u, 0x80021A20u,
    0x80021BA4u, 0x80021C38u, 0x80021DDCu, 0x80022048u, 0x80022340u, 0x80020A98u,
    0x8002258Cu, 0x800559BCu, 0x80055968u, 0x800207D4u,
    /* 3D: viewport, view build, cars, course, objects, course tail, mirror */
    0x8006F2B0u, 0x8006E4D0u, 0x8002E554u, 0x8002DC00u, 0x8006F00Cu, 0x80074AD8u,
    0x800374F8u, 0x80070A58u,
    /* Demo/replay setup, fade, text and replay badge (race handler). */
    0x800914B0u, 0x80014A84u, 0x800558E8u, 0x80020074u, 0x80090600u,
    0x8004CBACu, 0x80059E24u, 0x80096660u, 0x8004C8BCu, 0x8004CC40u,
};
#define N_FNS (sizeof k_draw_fns / sizeof k_draw_fns[0])
#define N_COMMON_FNS (N_FNS - 10u) /* final ten roots belong to demo/replay */
#define IDX_CLEAR_OT N_FNS   /* the main loop's ClearOTagR pair */
#define N_SLOTS (N_FNS + 1u)

enum { MODE_BASE = 0, MODE_NEST = 1, MODE_ISO = 2, MODE_POISON = 3 };

typedef struct FnStat {
    uint64_t calls, bytes;
    uint8_t *map;                 /* 1 bit per byte: RAM, then spad, then GTE */
    uint32_t ras[8];
    uint64_t ra_calls[8];
} FnStat;

#define MAP_BYTES(ram) (((ram) + 1024u + 256u) / 8u)

static struct {
    int registered, active, mode, regs_poison;
    int in_tick, depth, race;
    uint32_t handler;
    int race_mode;
    uint64_t seq;
    uint32_t ram;
    FILE *log;
    char summary_path[512];
    uint32_t skip[16];
    unsigned nskip;
    /* per-tick deltas */
    uint64_t t_spans, t_irq, t_mmio_w, t_mmio_r;
    FnStat fn[N_SLOTS];
    uint64_t sp_mismatch, nested_fail;
    uint32_t cur_fn;
    uint64_t dump_seq;
    uint32_t debug_addr;          /* R4_DRAW_ISO_DEBUG=addr[,ticks] */
    uint64_t debug_ticks;
} I;

static uint32_t rd32(uint32_t a) { return psx_mod_read_word(a); }
static uint16_t rd16(uint32_t a) { return psx_mod_read_half(a); }
/* What stock R4 would read: the merged view while isolating. */
static uint32_t mrd32(uint32_t a) {
    return psx_draw_iso_enabled() ? psx_draw_iso_read_word(a) : rd32(a);
}

/* Test-only driver from the owner's timing harness. Read the merged camera:
 * the gate intentionally keeps main's draw scratch private. */
#define r4_gate_ad_read mrd32
#include "r4_gate_autodrive.h"
static FILE *s_ad_log;

static void autodrive_reset(void) {
    const char *path = getenv("R4_DRAW_ISO_INPUT_LOG");
    r4_autodrive_reset();
    if (s_ad_log) { fclose(s_ad_log); s_ad_log = NULL; }
    if (r4_autodrive_enabled() && path && *path) s_ad_log = fopen(path, "w");
}

/* Test harness inputs run after frontend sampling, including headless/TCP
 * routes (ordinary controller transforms deliberately exclude those modes). */
#include "sio.h"
extern void mod_register_frame_hook(void (*hook)(void));
static void autodrive_input(void) {
    if (!I.active || !I.race || !r4_autodrive_enabled()) return;
    for (unsigned player = 0; player < (I.race_mode == R4_MODE_SPLIT ? 2u : 1u); ++player) {
        uint32_t buttons = r4_autodrive_buttons(player, I.race_mode == R4_MODE_SPLIT
            ? 0x80118520u + 16u * player : 0x1F800008u);
        if (!buttons) continue;
        sio_set_pad_state_slot((int)player, (uint16_t)buttons);
        if (s_ad_log) fprintf(s_ad_log, "%u:%04X\n", player, buttons);
    }
}

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

/* ---- per-function write map ----------------------------------------- */

static uint32_t map_index(uint32_t phys) {
    if (phys >= 0xFFFF0000u) return I.ram + 1024u + (phys - 0xFFFF0000u);
    if (phys >= 0x1F800000u) return I.ram + (phys - 0x1F800000u);
    return phys;
}

static void note_range(void *user, uint32_t phys, uint32_t len) {
    FnStat *f = (FnStat *)user;
    uint32_t base = map_index(phys);
    f->bytes += len;
    if (!f->map) return;
    for (uint32_t k = 0; k < len; k++) f->map[(base + k) >> 3] |= (uint8_t)(1u << ((base + k) & 7u));
}

static void note_ra(FnStat *f, uint32_t ra) {
    for (unsigned k = 0; k < 8; k++) {
        if (f->ras[k] == ra || !f->ras[k]) { f->ras[k] = ra; f->ra_calls[k]++; return; }
    }
}

static void write_map(FILE *o, const char *label, const uint8_t *map, uint32_t total) {
    uint32_t start = 0;
    int in = 0;
    fprintf(o, "%s", label);
    for (uint32_t b = 0; b <= total; b++) {
        int set = b < total && (map[b >> 3] >> (b & 7u) & 1u);
        if (set && !in) { start = b; in = 1; }
        else if (!set && in) {
            uint32_t a = start < I.ram ? 0x80000000u + start
                       : start < I.ram + 1024u ? 0x1F800000u + (start - I.ram)
                       : 0xFFFF0000u + (start - I.ram - 1024u);
            fprintf(o, " %08X+%X", a, b - start);
            in = 0;
        }
        if (!in && (b & 7u) == 0 && b + 8u <= total && !map[b >> 3]) b += 7u;
    }
    fprintf(o, "\n");
}

static uint8_t *s_owned_map, *s_shared_map;
static void note_owned(void *user, uint32_t phys, uint32_t len) {
    uint8_t *map = (uint8_t *)user;
    uint32_t base = map_index(phys);
    for (uint32_t k = 0; k < len; k++) map[(base + k) >> 3] |= (uint8_t)(1u << ((base + k) & 7u));
}

static void write_summary(void) {
    FILE *o;
    PsxDrawIsoStats st;
    uint32_t total = I.ram + 1024u + 256u;
    if (!I.summary_path[0] || !(o = fopen(I.summary_path, "w"))) return;
    psx_draw_iso_get_stats(&st);
    fprintf(o, "# r4 draw isolation summary: mode %d, ticks %llu, spans %llu, irq swaps %llu\n",
            I.mode, (unsigned long long)I.seq, (unsigned long long)st.spans,
            (unsigned long long)st.exception_swaps);
    fprintf(o, "# mmio in spans: writes %llu reads %llu (by class other/gpu/dma/irq/timer/cd/spu/sio:",
            (unsigned long long)st.mmio_writes, (unsigned long long)st.mmio_reads);
    for (unsigned c = 0; c < 8; c++) fprintf(o, " %llu", (unsigned long long)st.mmio_writes_by_class[c]);
    fprintf(o, ") last write %08X pc %08X, last read %08X\n", st.mmio_last_write_addr,
            st.mmio_last_write_pc, st.mmio_last_read_addr);
    fprintf(o, "# draw bytes ram/spad/gte %llu %llu %llu; main overwrote owned %llu %llu %llu; handoff %llu\n",
            (unsigned long long)st.draw_bytes[0], (unsigned long long)st.draw_bytes[1],
            (unsigned long long)st.draw_bytes[2], (unsigned long long)st.logic_overwrites[0],
            (unsigned long long)st.logic_overwrites[1], (unsigned long long)st.logic_overwrites[2],
            (unsigned long long)st.handoff_bytes);
    fprintf(o, "# sp mismatches %llu, nested failures %llu\n",
            (unsigned long long)I.sp_mismatch, (unsigned long long)I.nested_fail);
    for (unsigned i = 0; i < N_SLOTS; i++) {
        FnStat *f = &I.fn[i];
        char label[64];
        if (!f->calls) continue;
        fprintf(o, "fn %08X calls %llu bytes %llu ras", i < N_FNS ? k_draw_fns[i] : R4_CLEAR_OTAG_R,
                (unsigned long long)f->calls, (unsigned long long)f->bytes);
        for (unsigned k = 0; k < 8 && f->ras[k]; k++)
            fprintf(o, " %08X:%llu", f->ras[k], (unsigned long long)f->ra_calls[k]);
        fprintf(o, "\n");
        if (f->map) {
            snprintf(label, sizeof label, "map %08X", i < N_FNS ? k_draw_fns[i] : R4_CLEAR_OTAG_R);
            write_map(o, label, f->map, total);
        }
    }
    if (s_owned_map) write_map(o, "owned_ever", s_owned_map, total);
    if (s_shared_map) write_map(o, "owned_last_tick", s_shared_map, total);
    fclose(o);
}

/* ---- the nested, isolated call ---------------------------------------- */

static int skipped(uint32_t fn) {
    for (unsigned k = 0; k < I.nskip; k++) if (I.skip[k] == fn) return 1;
    return 0;
}

static void poison_regs(CPUState *cpu) {
    static const unsigned regs[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 24, 25};
    for (unsigned k = 0; k < sizeof regs / sizeof regs[0]; k++) cpu->gpr[regs[k]] ^= 0x5A5A5A5Au;
    cpu->hi ^= 0x5A5A5A5Au;
    cpu->lo ^= 0x5A5A5A5Au;
}

static void run_isolated(CPUState *cpu, uint32_t fn, unsigned slot) {
    const uint32_t ra = cpu->gpr[31], sp = cpu->gpr[29];
    FnStat *f = &I.fn[slot];
    int isolated = 0;
    uint32_t dbg_before = I.debug_addr ? rd32(I.debug_addr) : 0, dbg_in = 0, dbg_out = 0;
    I.depth = 1;
    if (I.mode >= MODE_ISO) isolated = psx_draw_iso_enter(cpu);
    if (I.debug_addr) dbg_in = rd32(I.debug_addr);
    psx_dispatch_call(cpu, fn, ra);
    if (I.debug_addr) dbg_out = rd32(I.debug_addr);
    if (isolated) {
        psx_draw_iso_leave(cpu);
        psx_draw_iso_last_span_ranges(note_range, f);
    }
    I.depth = 0;
    if (cpu->gpr[29] != sp) I.sp_mismatch++;
    if (((cpu->pc ^ ra) & 0x1FFFFFFFu) != 0u && cpu->pc != 0u) I.nested_fail++;
    cpu->gpr[31] = ra;
    if (isolated && I.regs_poison) poison_regs(cpu);
    if (I.debug_addr && I.seq < I.debug_ticks)
        fprintf(stderr, "[draw-iso] seq %llu fn %08X main %08X in %08X out %08X main-after %08X\n",
                (unsigned long long)I.seq, fn, dbg_before, dbg_in, dbg_out, rd32(I.debug_addr));
    f->calls++;
    note_ra(f, ra);
    (void)psx_mod_finish_function(cpu);
}

static int eligible(CPUState *cpu) {
    (void)cpu;
    return I.active && I.mode != MODE_BASE && !I.depth && I.in_tick && I.race &&
           !psx_get_in_exception();
}

static void on_draw_fn(CPUState *cpu, uint32_t address) {
    unsigned slot;
    uint32_t fn = address | 0x80000000u;
    if (!eligible(cpu)) return;
    for (slot = 0; slot < N_FNS && k_draw_fns[slot] != fn; slot++) {}
    if (slot == N_FNS || skipped(fn)) return;
    if (slot >= N_COMMON_FNS && I.race_mode != R4_MODE_DEMO &&
        I.race_mode != R4_MODE_REPLAY) return;
    run_isolated(cpu, fn, slot);
}

/* Sound and rand() use scalar arguments/results, but update state consumed
 * by main logic and IRQs. Execute their original recompiled routines on
 * main at the exact guest call point. */
static void on_main_fn(CPUState *cpu, uint32_t address) {
    uint32_t ra = cpu->gpr[31];
    if (!I.active || !I.depth || !psx_draw_iso_live() || psx_get_in_exception()) return;
    /* A failed handoff must stop this diagnostic. Falling through would run
     * sound or rand against the private image and report a bogus gate pass. */
    if (!psx_draw_iso_suspend(cpu)) {
        fprintf(stderr, "draw isolation: main call %08X could not suspend at %08X\n",
                address, cpu->pc);
        abort();
    }
    psx_dispatch_call(cpu, address | 0x80000000u, ra);
    psx_draw_iso_resume(cpu);
    if (!psx_draw_iso_live()) {
        fprintf(stderr, "draw isolation: main call %08X could not resume at %08X\n",
                address, cpu->pc);
        abort();
    }
    cpu->gpr[31] = ra;
    (void)psx_mod_finish_function(cpu);
}

/* ---- tick boundaries -------------------------------------------------- */

static void on_clear_otag(CPUState *cpu, uint32_t address) {
    (void)address;
    if (!I.active || I.depth) return;
    if (cpu->gpr[31] == R4_RA_CLEAR_OT1) {
        I.in_tick = 1;
        I.handler = r4_handler();
        I.race_mode = r4_mode_of(I.handler);
        I.race = I.race_mode != R4_MODE_NONE;
        /* Leaving the race hands the draw context back: publish all of it
         * (the sync point a threaded build needs at a mode change). */
        if (!I.race && psx_draw_iso_enabled()) psx_draw_iso_end();
    }
    if ((cpu->gpr[31] == R4_RA_CLEAR_OT1 || cpu->gpr[31] == R4_RA_CLEAR_OT2) &&
        I.mode != MODE_BASE && I.race && !skipped(R4_CLEAR_OTAG_R))
        run_isolated(cpu, R4_CLEAR_OTAG_R, IDX_CLEAR_OT);
}

/* Walk an OT like the GPU's linked-list DMA: hand every word it will read to
 * the main context and hash them in walk order. */
static uint64_t walk_ot(uint32_t head, uint32_t *words, uint64_t h) {
    uint32_t p = head & 0x1FFFFCu, guard = 0;
    while (guard++ < 0x40000u) {
        uint32_t w = mrd32(0x80000000u | p);
        uint32_t n = w >> 24;
        if (psx_draw_iso_enabled()) psx_draw_iso_handoff(0x80000000u | p, 4u * (n + 1u));
        h = (h ^ w) * 0x100000001B3ull;
        for (uint32_t k = 1; k <= n; k++) {
            uint32_t v = mrd32(0x80000000u | ((p + 4u * k) & 0x1FFFFCu));
            h = (h ^ v) * 0x100000001B3ull;
        }
        *words += n + 1u;
        if ((w & 0x00FFFFFFu) == 0x00FFFFFFu) break;
        p = w & 0x1FFFFCu;
    }
    return h;
}

static uint32_t owned_bytes_at(uint32_t addr, uint32_t len) {
    /* A main-loop read of a draw-owned byte: main would read poison. */
    uint32_t n = 0;
    if (!psx_draw_iso_enabled()) return 0;
    for (uint32_t k = 0; k < len; k += 4u)
        if (psx_draw_iso_read_word(addr + k) != rd32(addr + k)) n++;
    return n;
}

/* R4_DRAW_ISO_DUMP=<seq+1>: the merged RAM and scratchpad at that tick's
 * pass point, and the owned map, for diffing a failing run against base. */
static void dump_merged(void) {
    const char *dir = getenv("R4_DRAW_ISO_DUMP_DIR");
    char path[600];
    FILE *o;
    snprintf(path, sizeof path, "%s/merged_%llu.bin", dir && *dir ? dir : ".",
             (unsigned long long)I.seq);
    if (!(o = fopen(path, "wb"))) return;
    for (uint32_t a = 0; a < psx_ram_live_bytes(); a += 4u) {
        uint32_t v = mrd32(0x80000000u + a);
        fwrite(&v, 4, 1, o);
    }
    for (uint32_t a = 0; a < 1024u; a += 4u) {
        uint32_t v = mrd32(0x1F800000u + a);
        fwrite(&v, 4, 1, o);
    }
    fclose(o);
}

static void on_vsync(CPUState *cpu, uint32_t address) {
    PsxDrawIsoStats st;
    uint32_t buf, ot_words = 0, loop_owned, excl[4];
    uint64_t ot_hash = 0xCBF29CE484222325ull;
    (void)address;
    if (!I.active || I.depth) return;
    if (cpu->gpr[31] == R4_RA_VSYNC1) { I.in_tick = 0; return; }
    if (cpu->gpr[31] != R4_RA_PASS_POINT || cpu->gpr[4] != 0u || !I.race) return;
    I.in_tick = 0;
    psx_draw_iso_sync();

    /* The buffer this tick built; the main loop submits it after VSync(0). */
    buf = mrd32(R4_CUR_BUF);
    if (psx_draw_iso_enabled()) {
        psx_draw_iso_handoff(buf, 0x70u);  /* DRAWENV, DISPENV */
        /* The submitted frame's heap/OT pointers are output too. In
         * particular max-detail's DrawOTag hook reads the heap end to
         * budget the following frame; retaining main's stale pointer
         * changes its visibility governor. */
        psx_draw_iso_handoff(0x1F800000u, 8u);
    }
    for (uint32_t k = 0; k < 0x70u; k += 4u) ot_hash = (ot_hash ^ mrd32(buf + k)) * 0x100000001B3ull;
    ot_hash = walk_ot(buf + 0xB6Cu, &ot_words, ot_hash);
    if (mrd32(R4_OT2_ENABLED)) ot_hash = walk_ot(buf + 0x166Cu, &ot_words, ot_hash);
    /* What the rest of the main loop reads before the next tick. */
    loop_owned = owned_bytes_at(R4_CUR_BUF, 4) + owned_bytes_at(R4_OT2_ENABLED, 4) +
                 owned_bytes_at(R4_FRAME_COUNTER, 4) + owned_bytes_at(R4_PACING, 4);

    psx_draw_iso_get_stats(&st);
    {
        /* Judged RAM excludes garbage by design: the dead stack below the
         * main loop's frame and the kernel's saved registers of the last
         * interrupted context (their values follow dead registers and
         * uninitialised stack, which differ per context). */
        uint32_t sp = cpu->gpr[29] & 0x1FFFFCu;
        excl[0] = 0x80009088u; excl[1] = 0x80009128u;
        excl[2] = 0x80000000u | (sp > 0x10000u ? sp - 0x10000u : 0u); excl[3] = 0x80000000u | sp;
    }
    if (I.log) {
        fprintf(I.log,
                "%llu,%u,%u,%08X,%d,%u,%llu,%llu,%llu,%llu,%u,%u,%u,%u,%u,%016llX,%016llX,%016llX,%016llX,%016llX\n",
                (unsigned long long)I.seq, rd32(R4_FRAME_COUNTER), rd32(R4_TICK), I.handler,
                I.race_mode, rd32(R4_PHASE),
                (unsigned long long)(st.spans - I.t_spans),
                (unsigned long long)(st.exception_swaps - I.t_irq),
                (unsigned long long)(st.mmio_writes - I.t_mmio_w),
                (unsigned long long)(st.mmio_reads - I.t_mmio_r),
                st.owned_now[0], st.owned_now[1], st.owned_now[2], loop_owned, ot_words,
                (unsigned long long)ot_hash,
                (unsigned long long)psx_draw_iso_hash_excluding(cpu, PSX_DRAW_ISO_SEG_RAM, excl, 2),
                (unsigned long long)psx_draw_iso_hash(cpu, PSX_DRAW_ISO_SEG_SPAD),
                (unsigned long long)psx_draw_iso_hash(cpu, PSX_DRAW_ISO_SEG_GTE),
                (unsigned long long)psx_draw_iso_hash(cpu, PSX_DRAW_ISO_SEG_RAM));
        if ((I.seq & 63u) == 0) fflush(I.log);
    }
    I.t_spans = st.spans; I.t_irq = st.exception_swaps;
    I.t_mmio_w = st.mmio_writes; I.t_mmio_r = st.mmio_reads;
    if (psx_draw_iso_enabled() && s_owned_map) {
        memset(s_shared_map, 0, MAP_BYTES(I.ram));
        psx_draw_iso_owned_ranges(note_owned, s_shared_map);
        for (uint32_t k = 0; k < MAP_BYTES(I.ram); k++) s_owned_map[k] |= s_shared_map[k];
    }
    if (I.dump_seq && I.seq + 1u == I.dump_seq) dump_merged();
    I.seq++;
    if ((I.seq % 600u) == 0) write_summary();
}

/* ---- activation ------------------------------------------------------- */

static void parse_env(void) {
    const char *e = getenv("R4_DRAW_ISO_SKIP");
    I.nskip = 0;
    while (e && *e && I.nskip < 16) {
        char *end;
        unsigned long v = strtoul(e, &end, 16);
        if (end == e) break;
        I.skip[I.nskip++] = (uint32_t)v | 0x80000000u;
        e = *end ? end + 1 : end;
    }
    e = getenv("R4_DRAW_ISO_DUMP");
    I.dump_seq = e ? strtoull(e, NULL, 10) : 0;
    e = getenv("R4_DRAW_ISO_DEBUG");
    I.debug_addr = 0;
    if (e && *e) {
        char *end;
        I.debug_addr = (uint32_t)strtoul(e, &end, 16);
        I.debug_ticks = *end == ',' ? strtoull(end + 1, NULL, 10) : 4;
    }
    e = getenv("R4_DRAW_ISO_REGS");
    I.regs_poison = e && e[0] == '1';
}

static void apply_public(void) {
    const char *e = getenv("R4_DRAW_ISO_PUBLIC");
    while (e && *e) {
        char *end;
        unsigned long lo = strtoul(e, &end, 16), hi;
        if (end == e || *end != '-') break;
        e = end + 1;
        hi = strtoul(e, &end, 16);
        if (end == e) break;
        if (hi > lo) psx_draw_iso_set_public((uint32_t)lo, (uint32_t)(hi - lo), 1);
        e = *end ? end + 1 : end;
    }
}

static void activate(void) {
    char value[32] = "";
    const char *env = getenv("R4_DRAW_ISO_MODE");
    const char *path;
    if (!I.registered) return;
    if (I.log) { fclose(I.log); I.log = NULL; }
    write_summary();
    psx_draw_iso_end();
    I.active = 1;
    if (env && *env) snprintf(value, sizeof value, "%s", env);
    else psx_mod_option_value(ISO_PACKAGE, "draw-isolation", "mode", value, sizeof value);
    I.mode = !strcmp(value, "iso") ? MODE_ISO : !strcmp(value, "poison") ? MODE_POISON
           : !strcmp(value, "nest") ? MODE_NEST : MODE_BASE;
    parse_env();
    path = getenv("R4_DRAW_ISO_LOG");
    I.log = fopen(path && *path ? path : "draw_iso_ticks.csv", "w");
    path = getenv("R4_DRAW_ISO_SUMMARY");
    snprintf(I.summary_path, sizeof I.summary_path, "%s", path && *path ? path : "draw_iso_summary.txt");
    if (I.log)
        fprintf(I.log, "seq,counter,tick,handler,mode,phase,spans,irq,mmio_w,mmio_r,owned_ram,"
                       "owned_spad,owned_gte,loop_reads_owned,ot_words,ot_hash,ram_hash,spad_hash,gte_hash,"
                       "ram_hash_all\n");
    I.seq = 0;
    I.in_tick = I.depth = I.race = 0;
    I.t_spans = I.t_irq = I.t_mmio_w = I.t_mmio_r = 0;
    I.ram = 0;
    autodrive_reset();

}

/* Isolation starts lazily on the first race tick with a live CPU. */
static void ensure_begun(CPUState *cpu) {
    if (I.mode < MODE_ISO || psx_draw_iso_enabled()) return;
    if (!psx_draw_iso_begin(cpu, I.mode == MODE_POISON ? PSX_DRAW_ISO_POISON : 0)) {
        I.mode = MODE_NEST;
        return;
    }
    apply_public();
}

static void on_clear_otag_entry(CPUState *cpu, uint32_t address) {
    if (I.active && !I.ram) {
        I.ram = psx_ram_live_bytes();
        for (unsigned i = 0; i < N_SLOTS; i++) {
            free(I.fn[i].map);
            memset(&I.fn[i], 0, sizeof I.fn[i]);
            I.fn[i].map = calloc(MAP_BYTES(I.ram), 1);
        }
        free(s_owned_map); free(s_shared_map);
        s_owned_map = calloc(MAP_BYTES(I.ram), 1);
        s_shared_map = calloc(MAP_BYTES(I.ram), 1);
    }
    if (I.active && !I.depth && cpu->gpr[31] == R4_RA_CLEAR_OT1) ensure_begun(cpu);
    on_clear_otag(cpu, address);
}

static void at_exit(void) {
    if (I.log) { fflush(I.log); }
    if (s_ad_log) fflush(s_ad_log);
    write_summary();
}

PSX_MOD_CONSTRUCTOR(r4_draw_isolation_register) {
    int ok = 1;
    for (unsigned i = 0; i < N_FNS; i++)
        ok &= psx_mod_register_function_entry_plugin(ISO_PLUGIN, k_draw_fns[i], on_draw_fn);
    ok &= psx_mod_register_function_entry_plugin(ISO_PLUGIN, R4_CLEAR_OTAG_R, on_clear_otag_entry);
    ok &= psx_mod_register_function_entry_plugin(ISO_PLUGIN, R4_VSYNC, on_vsync);
    ok &= psx_mod_register_function_entry_plugin(ISO_PLUGIN, 0x8004F520u, on_main_fn);
    ok &= psx_mod_register_function_entry_plugin(ISO_PLUGIN, 0x8004F5E0u, on_main_fn);
    ok &= psx_mod_register_function_entry_plugin(ISO_PLUGIN, 0x80051840u, on_main_fn);
    ok &= psx_mod_register_function_entry_plugin(ISO_PLUGIN, 0x80051B00u, on_main_fn);
    ok &= psx_mod_register_function_entry_plugin(ISO_PLUGIN, 0x80096640u, on_main_fn);
    I.registered = ok;
    mod_register_frame_hook(autodrive_input);
    psx_mod_register_activation_plugin(ISO_PLUGIN, activate);
    psx_mod_register_savestate_plugin(ISO_PLUGIN, autodrive_reset);
    atexit(at_exit);
}

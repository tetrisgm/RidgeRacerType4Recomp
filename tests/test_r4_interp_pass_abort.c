/* R4 interpolation hooks after a rolled-back render pass (src/mods/r4_interp.c).
 *
 * The framework's pass watchdog leaves a runaway pass by longjmp, so r4_pass
 * never reaches its own return. The plugin's hooks must keep working on the
 * next tick: a stale "inside a pass" flag would switch interpolation off for
 * the rest of the process. The mod API here is a mock with a flat guest RAM;
 * the plugin is the real one, driven through the attract demo's gates.
 * Build/run: ctest -R r4_interp_pass_abort */
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#include "cpu_state.h"
#include "mod_plugins.h"
#include "r4_interp.h"

static int failures;
#define CHECK(c, m) do { if (!(c)) { fprintf(stderr, "FAIL: %s\n", m); failures++; } } while (0)

/* ---- guest memory ------------------------------------------------------ */
static uint8_t s_ram[2u * 1024u * 1024u];
static uint8_t s_spad[1024];

static uint8_t *at(uint32_t a) {
    if ((a & 0xFFFFFC00u) == 0x1F800000u) return &s_spad[a & 0x3FFu];
    return &s_ram[a & 0x1FFFFFu];
}
uint8_t psx_mod_read_byte(uint32_t a) { return *at(a); }
uint16_t psx_mod_read_half(uint32_t a) { uint16_t v; memcpy(&v, at(a), 2); return v; }
uint32_t psx_mod_read_word(uint32_t a) { uint32_t v; memcpy(&v, at(a), 4); return v; }
void psx_mod_write_half(uint32_t a, uint16_t v) { memcpy(at(a), &v, 2); }
void psx_mod_write_word(uint32_t a, uint32_t v) { memcpy(at(a), &v, 4); }

/* ---- hooks ------------------------------------------------------------- */
static PSXModFunctionEntryCallback s_vsync_hook, s_otag_hook;
int psx_mod_register_function_entry_plugin(const char *id, uint32_t address,
                                           PSXModFunctionEntryCallback cb) {
    (void)id;
    if (address == 0x8008B330u) s_vsync_hook = cb;
    if (address == 0x80093418u) s_otag_hook = cb;
    return 1;
}

/* ---- render passes: the framework's contract, with its watchdog -------- */
static jmp_buf s_watchdog;
static int s_in_fn, s_abort_next, s_passes, s_calls_in_pass;
static uint8_t s_ram_ck[sizeof s_ram], s_spad_ck[sizeof s_spad];

uint32_t psx_mod_render_pass_plan(uint32_t period, uint32_t shown,
                                  uint32_t *alpha_q16, uint32_t max) {
    (void)period; (void)shown;
    if (!alpha_q16 || max == 0) return 0;
    alpha_q16[0] = 32768u;
    return 1;
}

int psx_mod_render_pass(CPUState *cpu, const PSXModRenderPass *pass,
                        PSXModRenderPassFn fn, void *user) {
    CPUState ck = *cpu;
    volatile int ok = 0;
    s_passes++;
    memcpy(s_ram_ck, s_ram, sizeof s_ram);
    memcpy(s_spad_ck, s_spad, sizeof s_spad);
    if (setjmp(s_watchdog) == 0) {
        s_in_fn = 1;
        ok = fn(cpu, user, pass->alpha_q16) ? 1 : 0;
    }
    s_in_fn = 0;
    memcpy(s_ram, s_ram_ck, sizeof s_ram);
    memcpy(s_spad, s_spad_ck, sizeof s_spad);
    *cpu = ck;
    return ok;
}

void psx_dispatch_call(CPUState *cpu, uint32_t addr, uint32_t ra) {
    (void)cpu; (void)ra;
    if (!s_in_fn) return;
    s_calls_in_pass++;
    /* The guest's own ClearOTagR entry fires the plugin's entry hook. */
    if (addr == 0x80093418u && s_otag_hook) s_otag_hook(cpu, addr);
    if (s_abort_next && s_calls_in_pass >= 3) {
        s_abort_next = 0;
        longjmp(s_watchdog, 1);          /* runaway draw code: rolled back */
    }
}

/* ---- the attract demo, one 30 Hz tick at a time ------------------------ */
static uint32_t s_demo_tick = 10;

static void tick(CPUState *cpu) {
    psx_mod_write_word(0x800ACCC4u, s_demo_tick++);   /* demo tick */
    r4_interp_note_vblank();
    r4_interp_note_vblank();
    cpu->gpr[31] = 0x8001E764u;                        /* loop head: ClearOTagR */
    s_otag_hook(cpu, 0x80093418u);
    cpu->gpr[31] = 0x8001E7E4u;                        /* VSync(0) */
    cpu->gpr[4] = 0;
    s_vsync_hook(cpu, 0x8008B330u);
}

int main(void) {
    CPUState cpu;
    memset(&cpu, 0, sizeof cpu);
    cpu.gpr[29] = 0x801FFF00u;

    /* Handler lookup -> the attract demo; race pacing; not paused. */
    psx_mod_write_half(0x800F4E1Au, 0);
    psx_mod_write_half(0x800F3BD6u, 0);
    psx_mod_write_word(0x8009EBCCu, 0x80100000u);
    psx_mod_write_word(0x80100000u, 0x8005E118u);
    psx_mod_write_word(0x800AC794u, 0x180u);
    psx_mod_write_word(0x800ACCD0u, 0);                 /* demo submode */
    psx_mod_write_half(0x800AC754u, 0);                 /* no cars listed */
    psx_mod_write_half(0x800ADCA0u + 0x5Cu + 4u, 320);  /* DISPENV w, h */
    psx_mod_write_half(0x800ADCA0u + 0x5Cu + 6u, 240);
    psx_mod_write_half(0x800ADCA0u + 0x22778u + 0x5Cu + 4u, 320);
    psx_mod_write_half(0x800ADCA0u + 0x22778u + 0x5Cu + 6u, 240);

    r4_interp_activate(1);
    CHECK(s_vsync_hook && s_otag_hook, "both entry hooks registered");
    if (!s_vsync_hook || !s_otag_hook) return 1;

    tick(&cpu);                                         /* first tick: history */
    tick(&cpu);
    CHECK(s_passes == 1, "a continuous demo tick renders a pass");

    s_abort_next = 1;
    s_calls_in_pass = 0;
    tick(&cpu);
    CHECK(s_passes == 2 && !s_abort_next, "the next pass ran away and was rolled back");

    tick(&cpu);
    CHECK(s_passes == 3, "the tick after a rolled-back pass interpolates again");
    tick(&cpu);
    CHECK(s_passes == 4, "and keeps interpolating");

    /* A new session re-activates the plugin from any state. */
    s_abort_next = 1;
    s_calls_in_pass = 0;
    tick(&cpu);
    r4_interp_activate(1);
    tick(&cpu);
    tick(&cpu);
    CHECK(s_passes == 6, "activation starts clean after a rolled-back pass");

    printf(failures ? "FAILED (%d)\n" : "ALL PASS\n", failures);
    return failures ? 1 : 0;
}

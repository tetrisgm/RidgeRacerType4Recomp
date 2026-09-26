/* R4 interpolation plugin against a mock mod API (src/mods/r4_interp.c).
 *
 * 1. Rolled-back passes. The framework's pass watchdog leaves a runaway pass
 *    by longjmp, so r4_pass never reaches its own return. The plugin's hooks
 *    must keep working on the next tick: a stale "inside a pass" flag would
 *    switch interpolation off for the rest of the process.
 * 2. Fallback. When the framework cannot run passes (status NO_PRESENTER,
 *    BACKEND or DISABLED), or passes keep being refused, the presenter is
 *    switched to the player's frame blend, logged once, and passes are tried
 *    again when they are available (with a back-off after failed resumes).
 *    Budget shedding and transient refusals (fast-forward) never fall back.
 *
 * The mod API is a mock with a flat guest RAM; the plugin is the real one,
 * driven through the attract demo's gates. Build/run: ctest -R r4_interp_plugin */
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

/* ---- hooks: psxrecomp's registry contract ------------------------------ */
/* One hook per (plugin id, address), segment bits ignored; a repeat returns 0
 * (mod_packages.cpp mod_register_function_entry_plugin). The hook table is
 * the framework's; here the test calls the callbacks itself. */
static PSXModFunctionEntryCallback s_vsync_hook, s_otag_hook;
static int s_owner_ok = 1, s_register_calls, s_register_refused;
static struct { char id[32]; uint32_t addr; } s_registered[8];
static int s_n_registered;
int psx_mod_register_function_entry_plugin(const char *id, uint32_t address,
                                           PSXModFunctionEntryCallback cb) {
    s_register_calls++;
    if (!id || !address || !cb) return 0;
    if (strcmp(id, "r4.framerate") != 0) s_owner_ok = 0;
    for (int i = 0; i < s_n_registered; i++)
        if (strcmp(s_registered[i].id, id) == 0 &&
            ((s_registered[i].addr ^ address) & 0x1FFFFFFFu) == 0u) {
            s_register_refused++;
            return 0;
        }
    if (s_n_registered == 8) return 0;
    snprintf(s_registered[s_n_registered].id, sizeof s_registered[0].id, "%s", id);
    s_registered[s_n_registered++].addr = address;
    if (address == 0x8008B330u) s_vsync_hook = cb;
    if (address == 0x80093418u) s_otag_hook = cb;
    return 1;
}

/* ---- presenter blend --------------------------------------------------- */
static uint32_t s_blend = PSX_MOD_FRAME_INTERPOLATION_HOLD;
static int s_blend_calls;
int psx_mod_set_frame_interpolation_blend(uint32_t mode) {
    s_blend = mode;
    s_blend_calls++;
    return 1;
}

/* ---- render passes: the framework's contract, with its watchdog -------- */
static jmp_buf s_watchdog;
static int s_in_fn, s_abort_next, s_passes, s_calls_in_pass;
static uint8_t s_ram_ck[sizeof s_ram], s_spad_ck[sizeof s_spad];
/* Framework state the fallback reads: status, a plan shed for time, and
 * passes the framework refuses without running them. */
static uint32_t s_status = PSX_MOD_RENDER_PASS_READY;
static int s_shed, s_refuse_passes, s_plans;

uint32_t psx_mod_render_pass_status(void) { return s_status; }

uint32_t psx_mod_render_pass_plan(uint32_t period, uint32_t shown,
                                  uint32_t *alpha_q16, uint32_t max) {
    (void)period; (void)shown;
    if (!alpha_q16 || max == 0) return 0;
    s_plans++;
    if (s_status != PSX_MOD_RENDER_PASS_READY || s_shed) return 0;
    alpha_q16[0] = 32768u;
    return 1;
}

int psx_mod_render_pass(CPUState *cpu, const PSXModRenderPass *pass,
                        PSXModRenderPassFn fn, void *user) {
    CPUState ck = *cpu;
    volatile int ok = 0;
    if (s_refuse_passes) return 0;
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

    /* The package constructor registers the hooks once, under the manifest
     * [[plugin]] id (psxrecomp runs them only while the plan activates it). */
    CHECK(r4_interp_register_hooks() == 2, "constructor: both entry hooks accepted");
    CHECK(s_vsync_hook && s_otag_hook, "both entry hooks registered");
    CHECK(s_owner_ok, "hooks owned by the manifest plugin id r4.framerate");
    CHECK(r4_interp_register_hooks() == 2 && s_register_calls == 2,
          "a second register call changes nothing");
    if (!s_vsync_hook || !s_otag_hook) return 1;
    r4_interp_activate(1, PSX_MOD_FRAME_INTERPOLATION_LINEAR);
    CHECK(s_register_calls == 2 && s_register_refused == 0,
          "activation does not register again (a repeat would return 0)");
    CHECK(s_blend == PSX_MOD_FRAME_INTERPOLATION_HOLD && s_blend_calls == 0,
          "activation with registered hooks: no fallback");

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
    r4_interp_activate(1, PSX_MOD_FRAME_INTERPOLATION_LINEAR);
    tick(&cpu);
    tick(&cpu);
    CHECK(s_passes == 6, "activation starts clean after a rolled-back pass");
    CHECK(s_blend_calls == 0, "passes working: the presenter blend is never touched");

    /* ---- 2. fallback --------------------------------------------------- */
    /* Shed for time (plan empty, status READY) and fast-forward are not
     * reasons to leave interpolation. */
    s_shed = 1;
    for (int i = 0; i < 10; i++) tick(&cpu);
    s_shed = 0;
    s_status = PSX_MOD_RENDER_PASS_FAST_FORWARD;
    for (int i = 0; i < 10; i++) tick(&cpu);
    s_status = PSX_MOD_RENDER_PASS_READY;
    CHECK(s_blend_calls == 0, "budget shedding and fast-forward keep HOLD");
    tick(&cpu);
    tick(&cpu);
    CHECK(s_passes == 8, "passes resume after shedding and fast-forward");

    /* The renderer declines passes (a mode without them, e.g. a hi-res
     * window): frame blend after three race ticks, logged once. */
    s_status = PSX_MOD_RENDER_PASS_BACKEND;
    tick(&cpu);
    tick(&cpu);
    CHECK(s_blend == PSX_MOD_FRAME_INTERPOLATION_HOLD, "two misses: still holding");
    tick(&cpu);
    CHECK(s_blend == PSX_MOD_FRAME_INTERPOLATION_LINEAR && s_blend_calls == 1,
          "third miss: the presenter shows the player's frame blend");
    {
        int plans = s_plans;
        for (int i = 0; i < 40; i++) tick(&cpu);
        CHECK(s_plans == plans, "no plans while passes stay unavailable");
        CHECK(s_blend_calls == 1, "and no further blend switches");
    }
    /* Available again: back to passes (HOLD) on the next race tick. */
    s_status = PSX_MOD_RENDER_PASS_READY;
    {
        int passes = s_passes;
        tick(&cpu);
        CHECK(s_blend == PSX_MOD_FRAME_INTERPOLATION_HOLD && s_blend_calls == 2,
              "passes available again: HOLD restored");
        CHECK(s_passes == passes + 1, "and the same tick renders a pass");
    }

    /* Passes refused although the framework reports them available: fall
     * back, and retry only after a back-off that doubles per failure. */
    s_refuse_passes = 1;
    tick(&cpu); tick(&cpu); tick(&cpu);
    CHECK(s_blend == PSX_MOD_FRAME_INTERPOLATION_LINEAR && s_blend_calls == 3,
          "refused passes: frame blend");
    {
        int plans = s_plans, calls, t;
        for (t = 0; t < 200 && s_plans == plans; t++) tick(&cpu);
        CHECK(t == 60, "second fallback retries after 60 ticks (doubled back-off)");
        CHECK(s_blend == PSX_MOD_FRAME_INTERPOLATION_HOLD, "the retry holds again");
        tick(&cpu); tick(&cpu);
        calls = s_blend_calls;
        CHECK(calls == 5 && s_blend == PSX_MOD_FRAME_INTERPOLATION_LINEAR,
              "still refused: three misses and back to blend");
    }
    s_refuse_passes = 0;
    {
        int t, passes = s_passes;
        for (t = 0; t < 300 && s_passes == passes; t++) tick(&cpu);
        CHECK(s_passes > passes && s_blend == PSX_MOD_FRAME_INTERPOLATION_HOLD,
              "passes work again after the back-off: interpolating");
    }

    /* Disabled after faults, with the Sharp blend style chosen. */
    r4_interp_activate(1, PSX_MOD_FRAME_INTERPOLATION_MOTION_ADAPTIVE);
    s_status = PSX_MOD_RENDER_PASS_DISABLED;
    for (int i = 0; i < 5; i++) tick(&cpu);
    CHECK(s_blend == PSX_MOD_FRAME_INTERPOLATION_MOTION_ADAPTIVE,
          "fallback uses the player's blend style");
    s_status = PSX_MOD_RENDER_PASS_READY;

    /* A later session activates again (e.g. an offline rematch): nothing is
     * registered again, and the same hooks keep interpolating. */
    {
        int passes;
        /* As r4_frame_rate_activate does: HOLD first, then the plugin. */
        psx_mod_set_frame_interpolation_blend(PSX_MOD_FRAME_INTERPOLATION_HOLD);
        r4_interp_activate(1, PSX_MOD_FRAME_INTERPOLATION_LINEAR);
        CHECK(s_register_calls == 2 && s_register_refused == 0,
              "second activation: no registration, no refusal");
        tick(&cpu);
        tick(&cpu);
        passes = s_passes;
        for (int i = 0; i < 4; i++) tick(&cpu);
        CHECK(s_passes > passes && s_blend == PSX_MOD_FRAME_INTERPOLATION_HOLD,
              "second activation: interpolating with the same hooks");
    }

    /* Frame blend method: the hooks do nothing. */
    {
        int plans = s_plans, calls = s_blend_calls;
        r4_interp_activate(0, PSX_MOD_FRAME_INTERPOLATION_LINEAR);
        for (int i = 0; i < 5; i++) tick(&cpu);
        CHECK(s_plans == plans && s_blend_calls == calls,
              "method = blend: no plans, no blend switches");
    }

    printf(failures ? "FAILED (%d)\n" : "ALL PASS\n", failures);
    return failures ? 1 : 0;
}

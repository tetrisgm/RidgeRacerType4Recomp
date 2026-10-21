/* Draw replay guards: watchdog recovery, unavailable passes, and 2P gating. */
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
/* Framework state the fallback reads: status, a plan shed for time (always,
 * or every s_shed_every-th plan), and passes the framework refuses without
 * running them. */
static uint32_t s_status = PSX_MOD_RENDER_PASS_READY;
static int s_shed, s_shed_every, s_refuse_passes, s_plans;

uint32_t psx_mod_render_pass_status(void) { return s_status; }

uint32_t psx_mod_render_pass_plan(uint32_t period, uint32_t shown,
                                  uint32_t *alpha_q16, uint32_t max) {
    (void)period; (void)shown;
    if (!alpha_q16 || max == 0) return 0;
    s_plans++;
    if (s_status != PSX_MOD_RENDER_PASS_READY || s_shed) return 0;
    if (s_shed_every && s_plans % s_shed_every == 0) return 0;
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
static uint32_t s_race_tick = 100;

static void tick(CPUState *cpu) {
    psx_mod_write_word(0x800ACCC4u, s_demo_tick++);   /* demo tick */
    cpu->gpr[31] = 0x8001E764u;                        /* loop head: ClearOTagR */
    s_otag_hook(cpu, 0x80093418u);
    cpu->gpr[31] = 0x8001E7E4u;                        /* VSync(0) */
    cpu->gpr[4] = 0;
    s_vsync_hook(cpu, 0x8008B330u);
}

/* One VS race tick: the race tick counter (0x800F2F94) advances instead of
 * the demo's. */
static void race_tick(CPUState *cpu) {
    psx_mod_write_word(0x800F2F94u, s_race_tick++);
    cpu->gpr[31] = 0x8001E764u;
    s_otag_hook(cpu, 0x80093418u);
    cpu->gpr[31] = 0x8001E7E4u;
    cpu->gpr[4] = 0;
    s_vsync_hook(cpu, 0x8008B330u);
}

static int local_draws;
static int check_local_draw(CPUState *cpu, void *user, uint32_t alpha) {
    (void)cpu; (void)user;
    CHECK(alpha == 32768u, "local pass uses an interpolation phase");
    CHECK(psx_mod_read_word(0x800AC0B0u) == 150u, "local car interpolated");
    CHECK(psx_mod_read_word(0x1F800008u) == 150u, "captured own-view camera interpolated");
    local_draws++;
    return 1;
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
    if (!s_vsync_hook || !s_otag_hook) return 1;
    r4_interp_activate(1);
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
    r4_interp_activate(1);
    tick(&cpu);
    tick(&cpu);
    CHECK(s_passes == 6, "activation starts clean after a rolled-back pass");
    CHECK(s_blend_calls == 0, "passes working: the presenter blend is never touched");

    /* No image-based fallback even after a lasting backend refusal. */
    for (int reason = 1; reason <= 6; ++reason) {
        s_status = reason;
        for (int i = 0; i < 40; ++i) tick(&cpu);
    }
    CHECK(s_blend_calls == 0, "refusals never enable a crossfade");
    s_status = PSX_MOD_RENDER_PASS_READY;

    /* A later session activates again (e.g. an offline rematch): nothing is
     * registered again, and the same hooks keep interpolating. */
    {
        int passes;
        /* As r4_frame_rate_activate does: HOLD first, then the plugin. */
        psx_mod_set_frame_interpolation_blend(PSX_MOD_FRAME_INTERPOLATION_HOLD);
        r4_interp_activate(1);
        CHECK(s_register_calls == 2 && s_register_refused == 0,
              "second activation: no registration, no refusal");
        tick(&cpu);
        tick(&cpu);
        passes = s_passes;
        for (int i = 0; i < 4; i++) tick(&cpu);
        CHECK(s_passes > passes && s_blend == PSX_MOD_FRAME_INTERPOLATION_HOLD,
              "second activation: interpolating with the same hooks");
    }

    /* VS split screen: overlay 661's handler, identified by its first code
     * words, is interpolated during the race and not outside it. */
    {
        static const uint32_t sig[6] = { 0x3C04800Fu, 0x3C038010u, 0x8C822F94u,
                                         0x8C63F860u, 0x27BDFFC8u, 0xAFBF0034u };
        int passes;
        for (int i = 0; i < 6; i++) psx_mod_write_word(0x80114C30u + 4u * i, sig[i]);
        psx_mod_write_word(0x80100000u, 0x80114C30u);
        psx_mod_write_word(0x800FF860u, 2);              /* race phase: running */
        race_tick(&cpu);                                 /* mode change: no pass yet */
        passes = s_passes;
        for (int i = 0; i < 4; i++) race_tick(&cpu);
        CHECK(s_passes == passes + 4, "VS split screen: every race tick interpolates");
        psx_mod_write_word(0x800FF860u, 0);              /* before the start */
        passes = s_passes;
        for (int i = 0; i < 4; i++) race_tick(&cpu);
        CHECK(s_passes == passes, "VS split screen: no pass outside the race phases");
        psx_mod_write_word(0x800FF860u, 4);              /* after the finish */
        for (int i = 0; i < 4; i++) race_tick(&cpu);
        CHECK(s_passes == passes, "VS split screen: no pass after the race");
        psx_mod_write_word(0x80114C30u + 20u, 0);        /* another overlay loaded */
        psx_mod_write_word(0x800FF860u, 2);
        for (int i = 0; i < 4; i++) race_tick(&cpu);
        CHECK(s_passes == passes, "VS split screen: only overlay 661's handler");
        psx_mod_write_word(0x80100000u, 0x8005E118u);    /* back to the demo */
        tick(&cpu);
    }

    /* Local-only link rendering captures the computed camera before the base
     * sandbox restores the shared view. Extra passes must use that camera. */
    {
        PSXModRenderPass rect = {0};
        rect.struct_size = sizeof rect; rect.w = 320; rect.h = 240;
        r4_interp_activate(1);
        psx_mod_write_word(0x800FF860u, 2);
        psx_mod_write_half(0x800AC754u, 2);
        for (unsigned i = 1; i <= 2; i++) {
            /* Ranking can reorder the roster: history follows each pointer. */
            psx_mod_write_word(0x800FFDD0u, i == 1 ? 0x800AC0B0u : 0x800AC3D0u);
            psx_mod_write_word(0x800FFDD4u, i == 1 ? 0x800AC3D0u : 0x800AC0B0u);
            psx_mod_write_word(0x800F2F94u, i);
            psx_mod_write_word(0x800AC0B0u, i * 100u);
            r4_interp_local_begin(&cpu, 0);
            psx_mod_write_word(0x1F800008u, i * 100u);
            r4_interp_local_camera();
            psx_mod_write_word(0x1F800008u, 9000u); /* restored shared camera */
            r4_interp_local_end(&cpu, &rect, check_local_draw, NULL, 1);
        }
        CHECK(local_draws == 1, "local replay waits for two consecutive ticks");
        CHECK(psx_mod_read_word(0x800AC0B0u) == 200u &&
              psx_mod_read_word(0x1F800008u) == 9000u, "local pass restores shared fields");
        r4_interp_local_end(&cpu, &rect, check_local_draw, NULL, 0);
        CHECK(local_draws == 1, "failed base local draw cannot generate frames");
    }

    /* Disabled feature: hooks do nothing. */
    {
        int plans = s_plans, calls = s_blend_calls;
        r4_interp_activate(0);
        for (int i = 0; i < 5; i++) tick(&cpu);
        CHECK(s_plans == plans && s_blend_calls == calls,
              "disabled: no plans, no blend switches");
    }

    printf(failures ? "FAILED (%d)\n" : "ALL PASS\n", failures);
    return failures ? 1 : 0;
}

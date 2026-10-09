/* r4_modern_controls.c - "R4 Controls" (r4.modern-controls).
 *
 * Modern scheme (the default): a title pad transform (psxrecomp
 * psx_mod_set_pad_transform, stage 2b of the offline input layer). During a
 * race, on a gamepad with both trigger axes, P1/P2 are presented as a native
 * NeGcon: twist = left stick X, I = right trigger (gas), II = left trigger
 * (brake), L = 0, shaped by the response curves in r4_modern_controls.h
 * (docs/MODERN_CONTROLS.md) onto R4's calibrated pressure span every poll.
 * Square / Circle / R1 press whatever R4's own NeGcon button config uses
 * for shift down / shift up / camera view (D-pad Up / Down and B by
 * default); every other face, shoulder and D-pad bit reaches nothing.
 * Triangle (Y) is the Rewind shortcut: activation lets the runtime claim the
 * title's one-button Rewind binding ([controller] direct_shortcut in
 * game.toml) as a direct shortcut, so Y never reaches the game.
 * Outside races, in the pause menu and the attract demo, on keyboards and
 * on pads without both triggers, the pad passes through unchanged. R4
 * re-reads the pad ID every poll and re-derives its input mode when it
 * changes, so presenting the NeGcon only while driving is safe.
 *
 * Classic scheme, or the feature off: nothing is registered, so the input
 * path is the stock one, bit for bit, and the Rewind binding means
 * Select + Y.
 *
 * The runtime never runs the transform under netplay, rollback resim,
 * selfcheck replay or a plain debug override (mod_plugins.h). */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mod_plugins.h"
#include "r4_modern_controls.h"
#include "r4_widescreen_scene.h"

#define R4_MC_PACKAGE "r4.modern-controls"
#define R4_MC_FEATURE "modern-controls"
#define R4_MC_PLUGIN  "r4.modern-controls"

/* R4 RAM (US). The pause flag is 1 while the in-race pause menu is up (R4
 * pauses its sound voices with it: set at 0x8004FD24, cleared at
 * 0x8004FD6C). The button config holds three 16-byte blocks per port
 * (DualShock / digital, NeGcon, JogCon); in the NeGcon block +8 is shift up,
 * +10 shift down and +12 the camera view, in R4's byte-swapped pad word. The
 * pad calibration (16 bytes per port) holds the NeGcon I / II rest offsets at
 * +10 / +12 and the twist dead zone / range settings at +16 / +18, indices
 * into the EXE tables R4's pad reader uses (0x8004B728, 0x8004B6FC). */
#define R4_MC_PAUSED_ADDR     0x800F4F6Cu
#define R4_MC_BUTTON_CFG_ADDR 0x800F3128u
#define R4_MC_PAD_CAL_ADDR    0x800AD6E0u
#define R4_MC_TWIST_DZ_TABLE  0x800A0324u   /* u16 stride 4: 0 6 10 14 */
#define R4_MC_TWIST_MAX_TABLE 0x800A0170u   /* u16: 25 38 75 113 */
#define R4_MC_ATTRACT_HANDLER 0x8005E118u
/* Each player's car (speed s16 at car + 0x1D8) and a VBlank counter; R4's
 * race logic runs every second VBlank, so counter >> 1 is its frame. Players
 * 1 and 2 (offline ports, netplay seats 0 / 1) drive R4's P1 / P2 car objects
 * 0x320 apart; Link Battle seats 3 and 4 drive the roster's cars
 * (0x800FFDD0 + 4 * seat, as r4_link_netplay.c draws them). */
#define R4_MC_P1_CAR          0x800AC0B0u
#define R4_MC_CAR_BYTES       0x320u
#define R4_MC_CAR_SPEED       0x1D8u
#define R4_MC_CAR_ROSTER      0x800FFDD0u
#define R4_MC_VBLANK_COUNTER  0x800A6548u
#define R4_MC_TUNING_FEATURE  "controls-tuning"

static R4ModernTuning s_r4_mc_tuning = R4_MC_TUNING_DEFAULTS;

static uint32_t r4_mc_read_word(uint32_t address) {
    return psx_mod_read_word(address);
}

static uint16_t r4_mc_read_half(uint32_t address) {
    return psx_mod_read_half(address);
}

/* A race the player drives: not the attract demo, not paused. */
static int r4_mc_driving(void) {
    if (!r4_in_race(r4_mc_read_word, r4_mc_read_half)) return 0;
    if (r4_frame_handler(r4_mc_read_word, r4_mc_read_half) ==
        R4_MC_ATTRACT_HANDLER)
        return 0;
    return psx_mod_read_word(R4_MC_PAUSED_ADDR) == 0;
}

/* The car this player drives, 0 when none is mapped. */
static uint32_t r4_mc_player_car(uint32_t player) {
    uint32_t car;
    if (player < 2u) return R4_MC_P1_CAR + R4_MC_CAR_BYTES * player;
    if (player >= R4_MC_PLAYERS) return 0u;
    car = psx_mod_read_word(R4_MC_CAR_ROSTER + 4u * player);
    return (car & 0xFFE00003u) == 0x80000000u ? car : 0u;
}

static void r4_mc_negcon_config(uint32_t player, R4ModernNegcon *ng) {
    const uint32_t port = R4_MC_CONFIG_PORT(player);
    const uint32_t cfg = R4_MC_BUTTON_CFG_ADDR + port * 48u + 16u;
    const uint32_t cal = R4_MC_PAD_CAL_ADDR + port * 16u;
    uint32_t rest, idx;
    ng->upshift = r4_mc_cfg_to_sio(psx_mod_read_half(cfg + 8u), R4_MC_PAD_DOWN);
    ng->downshift = r4_mc_cfg_to_sio(psx_mod_read_half(cfg + 10u), R4_MC_PAD_UP);
    ng->view = r4_mc_cfg_to_sio(psx_mod_read_half(cfg + 12u), R4_MC_PAD_TRIANGLE);
    rest = psx_mod_read_half(cal + 10u);
    ng->rest_i = rest < 256u ? rest : 0u;
    rest = psx_mod_read_half(cal + 12u);
    ng->rest_ii = rest < 256u ? rest : 0u;
    idx = psx_mod_read_half(cal + 16u);
    ng->twist_deadzone = idx < 4u
        ? psx_mod_read_half(R4_MC_TWIST_DZ_TABLE + idx * 4u) : 6u;
    idx = psx_mod_read_half(cal + 18u);
    ng->twist_range = idx < 4u
        ? psx_mod_read_half(R4_MC_TWIST_MAX_TABLE + idx * 2u) : 38u;
    if (ng->twist_deadzone > 32u) ng->twist_deadzone = 6u;
    if (!ng->twist_range || ng->twist_range > 127u) ng->twist_range = 38u;
    {
        const uint32_t car = r4_mc_player_car(player);
        const int16_t speed = car
            ? (int16_t)psx_mod_read_half(car + R4_MC_CAR_SPEED) : 0;
        ng->speed = speed > 0 ? (uint32_t)speed : 0u;
    }
    ng->phase = psx_mod_read_word(R4_MC_VBLANK_COUNTER) >> 1;
    ng->tuning = s_r4_mc_tuning;
}

/* Hidden integer options; a missing or malformed value keeps the default. */
static void r4_mc_tuning_option(const char *id, uint32_t *field, uint32_t max) {
    char text[16];
    char *end = NULL;
    unsigned long v;
    if (!psx_mod_option_value(R4_MC_PACKAGE, R4_MC_TUNING_FEATURE, id,
                              text, sizeof text))
        return;
    v = strtoul(text, &end, 10);
    if (end != text && *end == '\0' && v <= max) *field = (uint32_t)v;
}

static void r4_mc_load_tuning(void) {
    R4ModernTuning t = R4_MC_TUNING_DEFAULTS;
    r4_mc_tuning_option("trigger_deadzone", &t.trigger_deadzone, 50u);
    r4_mc_tuning_option("trigger_full", &t.trigger_full, 100u);
    r4_mc_tuning_option("trigger_gamma", &t.trigger_gamma, 300u);
    r4_mc_tuning_option("throttle_low_share", &t.throttle_low_share, 99u);
    r4_mc_tuning_option("throttle_step_share", &t.throttle_step_share, 99u);
    r4_mc_tuning_option("launch_speed", &t.launch_speed, 1000u);
    r4_mc_tuning_option("launch_span", &t.launch_span, 2000u);
    r4_mc_tuning_option("feather_min", &t.feather_min, 52u);
    r4_mc_tuning_option("brake_min", &t.brake_min, 50u);
    r4_mc_tuning_option("stick_deadzone", &t.stick_deadzone, 50u);
    r4_mc_tuning_option("stick_ease", &t.stick_ease, 100u);
    r4_mc_tuning_option("steer_speed_cut", &t.steer_speed_cut, 90u);
    r4_mc_tuning_option("steer_speed_low", &t.steer_speed_low, 4000u);
    r4_mc_tuning_option("steer_speed_high", &t.steer_speed_high, 4000u);
    s_r4_mc_tuning = t;
}

static int r4_mc_transform(const PSXModPadFrame *frame, PSXModPadOutput *out) {
    R4ModernNegcon ng;
    if (!frame || frame->player >= R4_MC_PLAYERS || !r4_mc_driving())
        return 0;
    r4_mc_negcon_config(frame->player, &ng);
    return r4_modern_controls_map(frame, &ng, out);
}

int r4_modern_controls_scheme_is_classic(const char *value) {
    return value && strcmp(value, "classic") == 0;
}

static void r4_mc_activate(void) {
    char scheme[32];
    PSXModPadTransform xf;
    if (psx_mod_option_value(R4_MC_PACKAGE, R4_MC_FEATURE, "scheme",
                             scheme, sizeof scheme) &&
        r4_modern_controls_scheme_is_classic(scheme))
        return; /* Classic: the stock input path, untouched. */
    r4_mc_load_tuning();

    memset(&xf, 0, sizeof xf);
    xf.struct_size = sizeof xf;
    xf.allowed_types = R4_MC_ALLOWED_TYPES;
    /* Boot type until the first frame; the first pass-through frame then
     * presents the device's own type, long before R4 polls its pads. */
    xf.initial_type = PSX_MOD_PAD_DUALSHOCK;
    xf.transform = r4_mc_transform;
    for (uint32_t player = 0; player < R4_MC_PLAYERS; ++player)
        (void)psx_mod_set_pad_transform(player, &xf);
    (void)psx_mod_allow_direct_shortcut(PSX_MOD_SHORTCUT_REWIND);
}

PSX_MOD_CONSTRUCTOR(r4_modern_controls_register) {
    (void)psx_mod_register_activation_plugin(R4_MC_PLUGIN, r4_mc_activate);
}

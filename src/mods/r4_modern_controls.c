/* r4_modern_controls.c - "R4 Controls" (r4.modern-controls).
 *
 * Modern scheme (the default): a title pad transform (psxrecomp
 * psx_mod_set_pad_transform, stage 2b of the offline input layer). During a
 * race, on a gamepad with both trigger axes, P1/P2 are presented as a native
 * NeGcon: twist = left stick X, I = right trigger (gas), II = left trigger
 * (brake), L = 0, scaled onto R4's calibrated pressure span every poll.
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

/* r4_frame_rate_plugin.c - "R4 Frame Rate" (r4.enhancement.frame-rate).
 *
 * Presentation-only: the game keeps its stock 30 Hz race logic. The OpenGL
 * presenter runs at the chosen rate and follows the game's real frame flips
 * (psx_mod_set_frame_interpolation_source FLIP), so a 30 Hz race is spread
 * over its whole frame instead of every guest VBlank.
 *
 *   method = interpolate  r4_interp.c redraws the race between game frames in
 *                         framework render passes (true in-between positions),
 *                         only in time the game leaves free before its next
 *                         frame is presented. Wherever none fits, or passes
 *                         are unavailable, the game's own frame is shown as
 *                         is (HOLD): nothing blended, nothing delayed.
 *   method = blend        crossfade of finished frames (Smooth = linear,
 *                         Sharp = motion-adaptive), only by the player's
 *                         choice.
 *
 * Never calls psx_mod_set_native_vblank_rate: that speeds up the machine. */
#include "mod_plugins.h"
#include "r4_frame_rate_options.h"
#include "r4_interp.h"

#include <stdio.h>

#define R4_FR_PACKAGE "r4.enhancement.frame-rate"
#define R4_FR_FEATURE "frame-rate"
#define R4_FR_PLUGIN  "r4.framerate"

static void r4_frame_rate_activate(void) {
    char value[32];
    uint32_t blend;
    R4FrameRateConfig cfg;
    r4_frame_rate_config_defaults(&cfg);
    if (psx_mod_option_value(R4_FR_PACKAGE, R4_FR_FEATURE, "rate",
                             value, sizeof value))
        cfg.fps = r4_frame_rate_parse_rate(value);
    if (psx_mod_option_value(R4_FR_PACKAGE, R4_FR_FEATURE, "method",
                             value, sizeof value))
        cfg.method = r4_frame_rate_parse_method(value);
    if (psx_mod_option_value(R4_FR_PACKAGE, R4_FR_FEATURE, "blend",
                             value, sizeof value))
        cfg.blend = r4_frame_rate_parse_blend(value);

    blend = cfg.blend == R4_FRAME_RATE_SHARP
                ? PSX_MOD_FRAME_INTERPOLATION_MOTION_ADAPTIVE
                : PSX_MOD_FRAME_INTERPOLATION_LINEAR;
    psx_mod_set_frame_interpolation_source(PSX_MOD_FRAME_SOURCE_FLIP);
    /* Interpolated: hold the newest game frame wherever no pass image
     * applies; never a crossfade. Frame blend: the player's blend style. */
    psx_mod_set_frame_interpolation_blend(
        cfg.method == R4_FRAME_RATE_INTERPOLATE
            ? (uint32_t)PSX_MOD_FRAME_INTERPOLATION_HOLD : blend);
    psx_mod_set_frame_interpolation(cfg.fps);
    if (cfg.method == R4_FRAME_RATE_INTERPOLATE) {
        /* Both reset at every session start, so set them on each activation.
         * CHANGED: present only new pictures (a game frame or a pass image),
         * so a held frame costs no host time and the rate is a ceiling, not
         * a workload. LEFTOVER: passes only in host time the game leaves
         * free before its frame is due, stopped at that deadline. Frame
         * blend changes the picture at every output and runs no passes, so
         * it keeps the defaults. The #ifdefs keep an older runtime building
         * (PSX_MOD_FRAME_INTERPOLATION_UNLIMITED came with the present API). */
#ifdef PSX_MOD_FRAME_INTERPOLATION_UNLIMITED
        psx_mod_set_frame_interpolation_present(PSX_MOD_FRAME_PRESENT_CHANGED);
#endif
#ifdef PSX_MOD_RENDER_PASS_LEFTOVER
        psx_mod_set_render_pass_budget(PSX_MOD_RENDER_PASS_LEFTOVER);
#endif
    }
    r4_interp_activate(cfg.method == R4_FRAME_RATE_INTERPOLATE);
    {
        const char *how = cfg.method == R4_FRAME_RATE_INTERPOLATE
            ? "interpolated in leftover time, the game's frames elsewhere"
            : "frame blend";
        if (cfg.fps)
            fprintf(stdout, "r4: frame rate %u FPS, %s\n", (unsigned)cfg.fps, how);
        else
            fprintf(stdout, "r4: frame rate follows the display, %s\n", how);
    }
}

PSX_MOD_CONSTRUCTOR(r4_frame_rate_register) {
    /* Entry hooks first, once per process, owned by the same [[plugin]] id:
     * they run only while the mod plan activates it. */
    (void)r4_interp_register_hooks();
    psx_mod_register_activation_plugin(R4_FR_PLUGIN, r4_frame_rate_activate);
}

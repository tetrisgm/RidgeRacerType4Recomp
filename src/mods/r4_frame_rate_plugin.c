/* r4_frame_rate_plugin.c - "R4 Frame Rate" (r4.enhancement.frame-rate).
 *
 * Presentation-only: the game keeps its stock 30 Hz race logic. The OpenGL
 * presenter runs at the chosen rate and follows the game's real frame flips
 * (psx_mod_set_frame_interpolation_source FLIP), so a 30 Hz race is spread
 * over its whole frame instead of every guest VBlank.
 *
 *   method = interpolate  r4_interp.c redraws the race between game frames in
 *                         framework render passes (true in-between positions);
 *                         when no pass applies, the latest frame is held, so
 *                         nothing is ever shown later than stock. While the
 *                         framework cannot run passes at all, it shows the
 *                         frame blend below instead (logged once).
 *   method = blend        crossfade of finished frames (Smooth = linear,
 *                         Sharp = motion-adaptive).
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
    /* Interpolated: hold the newest frame wherever no pass image applies;
     * r4_interp switches to `blend` while passes are unavailable. */
    psx_mod_set_frame_interpolation_blend(
        cfg.method == R4_FRAME_RATE_INTERPOLATE
            ? (uint32_t)PSX_MOD_FRAME_INTERPOLATION_HOLD : blend);
    psx_mod_set_frame_interpolation(cfg.fps);
    r4_interp_activate(cfg.method == R4_FRAME_RATE_INTERPOLATE, blend);
    if (cfg.fps)
        fprintf(stdout, "r4: frame rate %u FPS, %s\n", (unsigned)cfg.fps,
                cfg.method == R4_FRAME_RATE_INTERPOLATE ? "interpolated"
                                                         : "frame blend");
    else
        fprintf(stdout, "r4: frame rate follows the display, %s\n",
                cfg.method == R4_FRAME_RATE_INTERPOLATE ? "interpolated"
                                                         : "frame blend");
}

PSX_MOD_CONSTRUCTOR(r4_frame_rate_register) {
    /* Entry hooks first, once per process, owned by the same [[plugin]] id:
     * they run only while the mod plan activates it. */
    (void)r4_interp_register_hooks();
    psx_mod_register_activation_plugin(R4_FR_PLUGIN, r4_frame_rate_activate);
}

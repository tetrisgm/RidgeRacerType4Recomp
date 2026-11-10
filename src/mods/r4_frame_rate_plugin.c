/* Presentation-only US R4 draw replay. Guest pacing remains stock.
 * Rate option: "60" (default) redraws an in-between frame per game tick;
 * "30" leaves the original frames alone (no passes, stock presenter). */
#include <string.h>

#include "mod_plugins.h"
#include "r4_interp.h"

#define R4_FR_PACKAGE "r4.enhancement.frame-rate"

static int hooks_ready, active;
static int rate_is_original(void) {
    char value[16];
    return psx_mod_option_value(R4_FR_PACKAGE, "frame-rate", "rate", value,
                                sizeof value) && strcmp(value, "30") == 0;
}
static void activate(void) {
    int ready = hooks_ready && !rate_is_original() &&
        psx_mod_activate_render_pass_rate(R4_FR_PACKAGE, "frame-rate", "rate",
                                          PSX_MOD_RENDER_PASS_FLIP_PENDING);
    active = ready;
    /* The restored producer is not wired to the current own-view bridge. */
    psx_mod_set_render_pass_netplay(0);
    r4_interp_activate(ready);
}

static void reset_history(void) { r4_interp_activate(0); }
static void restored(void) { r4_interp_activate(active); }

PSX_MOD_CONSTRUCTOR(r4_frame_rate_register) {
    hooks_ready = r4_interp_register_hooks() == 2;
    psx_mod_register_activation_plugin("r4.framerate", activate);
    psx_mod_register_session_reset_plugin("r4.framerate", reset_history);
    psx_mod_register_savestate_plugin("r4.framerate", restored);
}

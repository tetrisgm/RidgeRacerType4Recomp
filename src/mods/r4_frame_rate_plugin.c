/* Presentation-only US R4 draw replay. Guest pacing remains stock. */
#include "mod_plugins.h"
#include "r4_interp.h"

static int hooks_ready, active;
static int recorded;
static void activate(void) {
    int ready = hooks_ready && psx_mod_activate_render_pass_rate(
        "r4.enhancement.frame-rate", "frame-rate", "rate",
        PSX_MOD_RENDER_PASS_FLIP_PENDING);
    active = ready;
    recorded = ready && psx_mod_set_recorded_render_passes(1);
    psx_mod_set_render_pass_netplay(ready);
    r4_interp_activate(ready && !recorded);
}

static void reset_history(void) { r4_interp_activate(0); }
static void restored(void) { r4_interp_activate(active && !recorded); }

PSX_MOD_CONSTRUCTOR(r4_frame_rate_register) {
    hooks_ready = r4_interp_register_hooks() == 2;
    psx_mod_register_activation_plugin("r4.framerate", activate);
    psx_mod_register_session_reset_plugin("r4.framerate", reset_history);
    psx_mod_register_savestate_plugin("r4.framerate", restored);
}

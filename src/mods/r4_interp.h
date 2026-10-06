/* r4_interp.h - R4 (SLUS-00797) true interpolated frames: per-tick capture of
 * car and camera state and the per-mode draw-only pass sequences run inside
 * framework render passes (psx_mod_render_pass). See r4_interp.c. */
#ifndef R4_INTERP_H
#define R4_INTERP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Called once from the package's constructor: registers the two
 * function-entry hooks under the manifest [[plugin]] id "r4.framerate"
 * (psxrecomp runs them only while the mod plan activates that id). Later
 * calls change nothing. Returns how many psxrecomp accepted (2 = both). */
int r4_interp_register_hooks(void);

/* Called from the package's activation callback. enabled = method is
 * "interpolate" (the presenter is set to HOLD: wherever no pass image
 * applies, the game's own frame is shown). */
void r4_interp_activate(int enabled);

#ifdef __cplusplus
}
#endif

#endif

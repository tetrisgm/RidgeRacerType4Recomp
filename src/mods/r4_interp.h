/* r4_interp.h - R4 (SLUS-00797) true interpolated frames: per-tick capture of
 * car and camera state and the per-mode draw-only pass sequences run inside
 * framework render passes (psx_mod_render_pass). See r4_interp.c. */
#ifndef R4_INTERP_H
#define R4_INTERP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "mod_plugins.h"

/* Called once from the package's constructor: registers the two
 * function-entry hooks under the manifest [[plugin]] id "r4.framerate"
 * (psxrecomp runs them only while the mod plan activates that id). Returns how many psxrecomp accepted (2 = both). */
int r4_interp_register_hooks(void);

/* Activation resets history across sessions. */
void r4_interp_activate(int enabled);

/* Bracket the link bridge's base local draw; camera capture happens inside
 * that sandbox, end runs after restoration. Extra callbacks receive alpha>0. */
void r4_interp_local_begin(struct CPUState *cpu, unsigned seat);
void r4_interp_local_camera(void);
void r4_interp_local_end(struct CPUState *cpu, const PSXModRenderPass *rect,
                         PSXModRenderPassFn draw, void *user, int ok);

#ifdef __cplusplus
}
#endif

#endif

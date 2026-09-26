/* r4_interp.h - R4 (SLUS-00797) true interpolated frames: per-tick capture of
 * car and camera state and the per-mode draw-only pass sequences run inside
 * framework render passes (psx_mod_render_pass). See r4_interp.c. */
#ifndef R4_INTERP_H
#define R4_INTERP_H

#ifdef __cplusplus
extern "C" {
#endif

/* Called from the package's activation callback. enabled = method is
 * "interpolate". Registers the two function-entry hooks once per process. */
void r4_interp_activate(int enabled);
/* Called from the package's (plan-gated) VBlank callback. */
void r4_interp_note_vblank(void);

#ifdef __cplusplus
}
#endif

#endif

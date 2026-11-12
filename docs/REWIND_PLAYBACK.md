# Direct rewind playback

Hold Y/Triangle or F8 to rewind; release resumes the displayed point. Select/Back
opens the custom menu. Existing bindings and settings remain authoritative.
Rewind remains enabled in the Mac race package.

## Implementation

R4 selects exact command playback by default. Complete private render passes
retain their ordered commands, PGXP/depth state, native texture memory and HD
residency in a bounded 256 MiB compressed history. They redraw at the current
gameplay resolution. This is resolution-independent state, not video history,
thumbnail playback or fitted geometry. Other framework titles retain state
playback unless they opt in. `PSX_REWIND_COMMANDS=0` selects that diagnostic path;
`PSX_REWIND_VIDEO=1` is a separate diagnostic prototype, not the release backend.

Machine checkpoints use bounded compressed memory (1 GiB plus eight raw hot
checkpoints), at most four guest frames apart. Recorded pads/camera inputs and
process-local enhancement state reconstruct the exact selected tick on release.
Release uses the nearest earlier checkpoint, preserving the sampled input boundary.
Hidden reconstruction omits redundant private redraws and presentation; guest
execution, canonical GPU work and the exact selected-state check remain intact.
The guest CPU stays frozen during backward command playback. No unused thumbnail
capture or large thumbnail-array moves occur on this path.

Release keeps the selected full-quality image visible until a complete forward
producer is ready. Native VRAM restaging cannot reach presentation; this prevents
the former pixelated center and doubled HUD. Future history is truncated when
resuming. Manual save/load remains independent.

## Verification and limits

The current code passed three successive history branches with zero differing
RAM words and 88 exact original/replayed full internal images with zero
mismatches (`analysis/release-pgxp-proof-20261111/result.json`). Diagnostic
readbacks are excluded from performance claims. Real-GL fixtures cover pixels
and live CPU/GPU/color/depth/stencil isolation at 1x/4x/9x, including changed
textures and PGXP triangles. The title suite now includes the PGXP propagation and keyboard data-path
regression fixtures; final test results are recorded in the release receipt.

Release timing now observes the first complete forward image without draining
the render queue. Earlier coarse polling and CPU-only transition measurements
must not be compared as if they measured the same endpoint. PBO restaging and
shorter replay remove redundant work; they do not establish zero latency.

See the [current release receipt](recovery/MAC_RELEASE_20261111.json) for exact
binary, performance and package status, and [handoff](HANDOFF.md) for pending
gates. Internal rendering is 3834×2160; the test monitor is 1440p/75 Hz. Tunnel,
sky, ground and HUD samples are bounded visual evidence, not an all-course
or all-hardware guarantee. Physical Steam Controller testing remains separate.
The [previous playtest receipt](recovery/MAC_60FPS_20261111.json) is historical.

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
checkpoints). Capture requests every snapshot-safe guest boundary; IRQ and
callback-stack safety gates still defer unsafe captures. Recorded pads/camera
inputs and process-local enhancement state reconstruct the exact selected tick
from the nearest earlier checkpoint. The guest CPU stays frozen during backward
command playback. No unused thumbnail capture occurs on this path.

Release primes presentation geometry while keeping the selected full-quality
image visible. It restores pending complete producers only from the current
branch's commands issued by the restored machine cycle. Two full-resolution
working textures retain recently redrawn producers, avoiding another draw when
reversing direction; this is a small rendering cache, not pixel history.

The restored pending image can be published as part of commit. The presentation
and history gates stay frozen until normal forward rendering reaches that
producer's tick, cycle and phase: native VRAM restaging, an older complete image,
or an earlier phase cannot replace it. This preserves image ordering and prevents
the former pixelated center/doubled HUD. Full-rate planning uses the same guest
cadence on the renderer and main threads. If eligible producer history is absent,
the existing bounded forward refill remains the fallback. Future branch references
are invalidated; manual save/load and the custom menu remain independent.

## Verification and limits

The current code passed three successive history branches with zero differing
RAM words and 130 exact original/replayed full internal images with zero
mismatches (`analysis/rewind-final-cocoa-exact-20261112/result.json`). Diagnostic
readbacks are excluded from performance claims. Real-GL fixtures cover pixels
and live CPU/GPU/color/depth/stencil isolation at 1x/4x/9x, including changed
textures and PGXP triangles. The title suite now includes the PGXP propagation and keyboard data-path
regression fixtures; final test results are recorded in the release receipt.

Release timing now observes the first complete forward image without draining
the render queue. Earlier coarse polling and CPU-only transition measurements
must not be compared as if they measured the same endpoint. PBO restaging and
shorter replay remove redundant work; they do not establish zero latency.

See the [current release receipt](recovery/MAC_RELEASE_20261112.json) for exact
binary, performance and package status, and [handoff](HANDOFF.md) for pending
gates. Internal rendering is 3834×2160; the test monitor is 1440p/75 Hz. Tunnel,
sky, ground and HUD samples are bounded visual evidence, not an all-course
or all-hardware guarantee. Physical Steam Controller testing remains separate.
The [previous playtest receipt](recovery/MAC_60FPS_20261111.json) is historical.

# Direct rewind playback

Hold Y/Triangle or F8 to rewind; release resumes the displayed point. Select/Back
opens the custom menu. Existing bindings and settings remain authoritative.
Rewind remains enabled in the local race package.

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
The guest CPU stays frozen during backward command playback. No unused thumbnail
capture or large thumbnail-array moves occur on this path.

Release keeps the selected full-quality image visible until a complete forward
producer is ready. Native VRAM restaging cannot reach presentation; this prevents
the former pixelated center and doubled HUD. Future history is truncated when
resuming. Manual save/load remains independent.

## Verification and limits

- `analysis/60fps-20261111-command-release-proof/result.json`: 24 original versus
  replayed full internal images identical; selected tick 1710 restored with zero
  differing RAM words. Diagnostic readbacks are excluded from performance claims.
- `analysis/60fps-20261111-command-visible/result.json`: visible Helter Skelter
  race, 2160 internal lines, 58.92 forward FPS over 120 seconds and 59.57 backward
  updates/s. The physical screen is 1440p/75 Hz. Tunnel, sky, ground and HUD were
  inspected; no center-resolution collapse or obvious seam/order faults observed.
- `analysis/60fps-20261111-command-release-profile/result.json`: 60.01 forward FPS
  over 30 seconds, 58.99 backward updates/s. Long-hold release took 0.38–0.51 s
  across the timing runs, including up to 100 ms of observation polling. This
  brief transition pause remains a limitation.
- Real-GL fixtures cover exact pixels and live CPU/GPU/color/depth/stencil state
  isolation at 1x/4x/9x, including changed textures and PGXP triangles.

This is bounded course/build evidence, not an all-course guarantee or proof of
perfect sustained 60 FPS. The owner requested wrapping up rather than more
repeated performance runs. See [handoff](HANDOFF.md) and [Mac build](MAC_BUILD.md).

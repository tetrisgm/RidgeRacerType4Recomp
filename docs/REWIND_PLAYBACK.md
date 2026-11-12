# Direct rewind playback

Owner requirement, 2026-11-11: rewind is a core release feature. It remains
enabled. Holding the configured rewind input must immediately play the race
backwards, without a filmstrip/menu or reduced-resolution preview; releasing
resumes from the displayed point. Select/Back continues to open the custom game
menu. Hold/release is the implementation default, not a changed owner binding.

## State history and presentation

The default implementation stores machine and engine state in memory, independent
of output resolution. A bounded compression worker retains up to 1 GiB of machine
snapshots, with eight raw hot checkpoints. CPU/device capture and queued native
VRAM capture publish only when complete. Stable process-local enhancement regions
retain interpolation endpoints, maximum-detail pacing, widescreen tracking,
camera state and the host frame counter. These regions do not change disk/wire
savestate formats. Recorded input includes physical camera axes and resolved pads.

Restoring native VRAM alone cannot recreate the enhanced image: it produced the
owner-reported pixelated center and doubled HUD. Backward presentation now tracks
complete private redraws by guest tick, producer cycle and phase. Replay rebuilds
that image at the current resolution before exposing the rewind point. Native
VRAM restages cannot pass the presentation barrier. Missing redraw recovery is
bounded by recorded input and four adjacent guest boundaries; failure is reported
instead of replaying indefinitely. Release resumes the accepted historical state
and truncates future history.

The optional `--rewind-video` qualification flag selects an earlier pixel-cache
prototype for diagnostics. It is not the default or the intended release design.
No resolution reduction or thumbnail playback is permitted.

## Evidence and outstanding acceptance

`analysis/60fps-20261111-complete-proof/rewind-{0..5}.png` contains accepted
3840×2160 backward frames without the previous center-resolution collapse.
`analysis/60fps-20261111-complete-ram-proof/result.json` reports zero differing
RAM words at release (CRC 3551031831), no pixel history, and 60.07 FPS over 30 s.
Those observations precede the bounded failure guard; final-build checks remain
necessary. They do not establish all-course visual correctness or rewind cadence.

The Mac was locked during the attempted visible run. Complete on-screen motion
verification after unlock: tunnel, sky, ground, hold/release transitions, repeated
history branches, controller Y/Triangle and Select/Back menu coexistence. Measure
120 seconds with rewind enabled separately from screenshots and the optional
`--verify-state` RAM diagnostic. Keep release qualification false until these
checks pass. See [handoff](HANDOFF.md) and [Mac workflow](MAC_BUILD.md).

# Separate-renderer development

The 4K/60 goal is not qualified. The custom Select/Back menu, settings,
controls and Steam Controller adapter remain in place. No release was made.

## Implemented

The engine now has host-owned scene snapshots, stable object identity and
topology/lifetime checks, camera/object interpolation before projection,
near-plane clipping, immutable GPU texture snapshots and an independent
offscreen triangle renderer. This adapts the preserved MStan/Ape pilot
`1b84d4fd`; it does not make that pilot an R4 scene producer. The independent
backend is exercised by fixtures and is not connected to the game presenter.
R4 geometry/material extraction, full depth/bloom/HD behavior and presentation
integration remain necessary before it can replace the current race renderer.

Two measured-path improvements retain the existing geometry and ordering:

- Opaque-only HLE batches use the existing fused color/bloom draw. Mixed
  translucent batches retain their existing blend equations. Opaque runs split
  by 2D subtractive draws now clear bloom classification like LLE; the former
  HLE path left background bloom marked. The fixture compares
  opaque color/depth/bloom against LLE and mixed draws against the previous HLE
  implementation at 1x, 4x and 9x.
- Car-effect lighting uses the same weighted pixels through nonblocking GPU
  transform feedback instead of default-framebuffer ReadPixels. Its result and
  GL state preservation are tested against the former CPU calculation. This
  removes a synchronous readback site, but alone did not improve sustained FPS.

## Full-redraw target experiment

`PSX_RENDER_PASS_FRESH_TARGET=1` is debug-only and off by default. It lets the
existing R4 guest draw sequence render into cleared private color/depth/stencil
surfaces while retaining native surfaces. CPU/device rollback and the small
raw VRAM mirror remain transactional. This is not the independent scene
producer described above, and partial-frame/old-image reads are not qualified.

The first version accidentally supported only y=0. R4 alternates y=0 and
y=240: this both retained expensive copies on half the frames and alternated
full-size private allocations with cropped backup allocations. The correction
uses full VRAM coordinates for both bands. The GL fixture checks that both
actually enter the private path and that canonical/wide color, depth, stencil
and PGXP grouping are restored exactly.

## Evidence and limits

Release and all 38 CTests pass with the retained margin optimization enabled
by default. Earlier two-band checkpoints passed the then-existing 37 tests.
The extended private-target/lighting fixture passes 144 checks; opaque batching
passes 42 in its expanded form, which enables precise PGXP coordinates
explicitly and compares all mixed blend modes (using LLE for the corrected
2D subtractive bloom case). Isolated
custom-menu, settings persistence and full boot-to-race
smoke passed with zero dispatch misses before the two-band correction, which
only affects the opt-in debug experiment.

Before that correction, the current-code 120-second fixed-9x race achieved
36.33 VBlanks/s, with five-second present samples of 29.45–41.84 FPS. Redraw
shedding was disabled; no dispatch misses occurred. This fails the target.
An earlier 15-second 51.03 result did not establish sustained performance.

The two-band correction measured **47.38 VBlanks/s over 120 seconds**, with
39.14–56.41 FPS presentation samples. This is useful progress, still below the
target. An additional all-3D translucent bloom-fusion probe passed image/depth
checks but measured 45.62 VBlanks/s; it was removed because no sustained gain
was established. These sequential runs had uncontrolled background activity;
the difference is not a hardware-isolated causal estimate.
[Condensed measurements and executable hashes](SEPARATE_RENDERER_20261111.json).

Local receipts: `analysis/60fps-20261111-current-throughput/`,
`analysis/separate-renderer-20261111-menu-smoke/`, and
`build/gl-presentation-results/receipt.json`. Earlier probes and ten matched
5040x2160 reference/experimental image pairs are under
`analysis/60fps-20261110/`. Those pairs predate the two-band correction and do
not establish continuous-motion freedom from blinking or 60 FPS performance.

## GPU trace and further probes

A five-second Metal System Trace of the two-band build records 4,162 ms of
fragment intervals versus 456 ms of vertex intervals for the runtime over a
roughly 5,700 ms span. These are interval unions, not shader occupancy or
independent channel timings. WindowServer activity overlaps some intervals.
The trace supports prioritizing fragment work; it does not justify lowering
quality. The race retains texture LOD mode 1 and anisotropy 16.
Local trace: `analysis/60fps-20261111-metal-trace-final/`.

A shader specialization preserving HD sampling, LOD and anisotropy measured
47.22 VBlanks/s; no useful gain was established, so it was removed.
The queued widescreen margin scissor avoids shading center pixels that
are later replaced by the canonical framebuffer. Anchored/full-composite views
retain full-width draws. The 120-second run reached **55.41 VBlanks/s**;
five-second presentation samples ranged 47.33–61.56. This still fails steady
60. A subsequent direct center/margin capture probe reached 55.70, too close
to establish a useful gain, and was removed. The 1x/4x/9x fixture compares completed color and observable depth/masking
for both vertical buffers (75 checks). `PSX_GL_WIDE_MARGINS_ONLY=0` selects
full replay for comparison. The subsequent six same-state 5040x2160 RGB pairs
before/inside/after the tunnel also match exactly with margin clipping enabled
only on the experimental pass. A visible race and twelve-image tunnel sequence
show intact wall/ceiling/road joins, sky, ground and HUD in those samples.
Receipts: `analysis/60fps-20261111-margin-capture-equivalence/` and
`analysis/60fps-20261111-margin-capture-motion/`. The direct-capture probe was
removed before these runs; the directory names retain its test-harness name.

## Visual checks of the two-band build

Six same-state 5040x2160 RGB pairs before, inside and after the tunnel were
pixel-identical between transactional reference and fresh-target rendering.
A visible race run also captured twelve consecutive native/intermediate tunnel
images; inspection of their wall/ceiling/road joins found no gaps or blinking
in that bounded sequence. Sky, ground and HUD captures looked intact. This is
not exhaustive continuous-motion qualification, and predates the margin-scissor
and direct-capture probes.
Receipts: `analysis/60fps-20261111-tunnel-equivalence-v2/` and
`analysis/60fps-20261111-tunnel-motion/`.

## Checkpoint and hidden-window test correction

Engine implementation: `1f5030c583c4b109cf3a47f3dd1a69f28e72148e`; final pin
`c593b788` also registers the shared test module with the recompiler build guard.
Supported regeneration leaves generated C unchanged and reuses all 7,459 native
overlay variants.
The final integrated menu, controls/settings persistence and boot-to-race smoke
passes with 7,175,443 linked native overlay calls and zero dispatch misses:
`analysis/separate-renderer-20261111-menu-final-vsync/`.

An initial final smoke timed out while the menu rendered at about 0.88 FPS.
Disabling margin clipping reproduced the slowdown before any race rendering.
A three-second CPU sample placed 1,270/1,323 main-thread samples in Cocoa's
VSync condition wait. The hidden-window harness now explicitly sets
`PSX_VSYNC=0`, like the existing performance harnesses; menu rendering resumed
above 80 FPS and the full script passed. Player settings and visible-game
VSync behavior are unchanged. This fixes automation, not race performance.

## Remaining-cost profile and rejected storage probe

On the retained build, a CPU sample with GPU timer queries disabled still
places the largest render-owner stack in Cocoa/OpenGL presentation, waiting
on Metal submission semaphores. The query-management stalls seen with
`PSX_GL_PERF=1` disappear, but the wait moves to normal presentation. This is
a bounded CPU sample, not GPU occupancy or proof that replacing guest logic
alone would reach 60. Receipts: `analysis/60fps-20261111-margins-remaining-cost/`
and `analysis/60fps-20261111-margins-cpu-only-v2/`.

A debug-only probe narrowed the private canonical target to the redraw's
columns without reducing the rendered resolution. It passed the 144-check
restoration fixture and initially measured 57.83 VBlanks/s. A subsequent
full-sized reference measured 47.87 and a smaller-target repeat measured
46.85. These runs do not establish a repeatable gain; the probe was removed
and the verified `c593b788` runtime rebuilt. The variability also means the
55.41 result must not be presented as an unconditional performance guarantee.
No cropped-target race-image qualification or 60 FPS package is claimed.

## Additional target and depth-clear probes

A one-target private redraw eliminated canonical triangle draws and rendered
its center through the wide target. It measured 56.99 VBlanks/s over 120 seconds
at 5040x2160, with five-second samples down to 47.10. Six same-state reference
pairs differed at 96–590 pixels; changing the center to the canonical viewport
reduced the differences to 39–380 pixels but did not eliminate them. These
experiments were removed. They do not qualify a new raster grid or steady 60.
Receipts: `analysis/60fps-20261111-single-target-throughput/`,
`analysis/60fps-20261111-single-target-equivalence/`, and
`analysis/60fps-20261111-single-grid-equivalence/`.

The retained checkpoint was also measured at standard 16:9, fixed 9x
(3834x2160). It reached **59.38 VBlanks/s over 120 seconds**, with five-second
presentation samples of 54.17–61.15. This is closer than ultrawide but still
contains slowdowns; it does not qualify locked 4K/60. Fresh targets were enabled,
redraw shedding and dynamic resolution disabled, and dispatch misses were zero.
Receipt: `analysis/60fps-20261111-standard-4k-throughput/`.

Depth resets already use hardware clears; the queued polygon records are not
executed as full-screen fragment draws. A subsequent conservative-bound probe
restricted repeated wide depth clears to preceding depth-write bounds within
one replay. It passed 99 real-GL color/depth/stencil checks at 1x/4x/9x and six
same-state full-resolution race pairs matched exactly. Inspection of tunnel
and exit images found no visible gaps or misplaced HUD in those stills. Its
120-second ultrawide throughput was 51.46 VBlanks/s (43.12–59.80 presentation
samples), which did not establish a useful gain. It was removed, including
its probe-specific fixture extension. Receipts and preserved local patches:
`analysis/60fps-20261111-depth-bounds-equivalence/`,
`analysis/60fps-20261111-depth-bounds-throughput/`, and
`analysis/60fps-20261111-depth-bounds-probe.patch`.

All these measurements share the earlier host-variability limitation. No
probe is enabled in the retained runtime, no independent R4 producer was
connected, and no new package or continuous-motion certification is claimed.

The restored runtime passes Release and all 38 CTests again. Final isolated
menu/controls/settings and boot-to-race verification passed with 7,180,361
native overlay calls and zero dispatch misses:
`analysis/separate-renderer-20261111-probes-final-smoke/native-overlay-check.json`.
Engine `5d379876` only records these results; no runtime code changed from
`c593b788`. Supported regeneration again reused all 7,459 overlay variants
and left generated C unchanged.

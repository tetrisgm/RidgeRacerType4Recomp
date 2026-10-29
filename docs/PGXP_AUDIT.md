# Phase 3b: PGXP audit

## Outcome and scope

The paused race no longer alternates between two enhanced images. PGXP,
perspective texturing, perspective colour, depth, preserved projection, and seam
corrections stay enabled. Changes are generic framework precision/seam/shader
work. `gpu_gl_renderer.c` is touched, including a small local-coordinate sampling
change in `present_target_quad`; presentation queues, threading and frame
scheduling are not restructured.

Two acceptance conditions remain open: paused forced-particle animation in the
title effect code, and the strict one-native-pixel distance gate. The latter is
a documented exception, not a claimed pass: preserved projection intentionally recovers information discarded
by integer GTE arithmetic. See the accuracy section below.

## Matched before/after evidence

Hardware: NVIDIA RTX 4080 SUPER, OpenGL 3.3, driver 621.01. Hidden window,
`SDL_AUDIODRIVER=dummy`, debug port 4898, 9x internal rendering and 5120x2880
composed captures. Port 4798 was found occupied by another worktree, so the audit
moved to 4898. Only processes launched by the audit were terminated.

The comparison loads the same isolated paused race save (car visible, Car Effects
active). For the baseline, only the renderer and seam header were temporarily
restored from framework `cc5638e8`; current counters remained. Both files were
restored before the final build. The baseline and final native hashes and
normalized PGXP stream hashes match. No PGXP feature was disabled.

| Measurement | Previous renderer | Fixed renderer |
|---|---:|---:|
| Enhanced A/B differing pixels | 3,452 | 0 |
| High-resolution source A/B differing pixels | 264 | 0 |
| Presented image hashes | 2 | 1 |
| Native image hashes | 1 | 1 |
| Normalized PGXP stream hashes | 1 | 1 |

The final composed-image check contains four bursts of eight **consecutive**
presents across two savestate loads, plus eight before and eight after accepting
an earlier rewind snapshot. All 48 match. Four replay samples each contain 860
triangles; empty VBlanks are excluded. Car Effects recorded 565 additional draw
callbacks during the two-load replay check. The previous supervisor-provided
checkpoint differed at 3,379 output pixels; the 3,452 figure is the stronger
matched-checkpoint comparison from this continuation.

- Presented PNG SHA-256: `ee6d8cc053047c843cc6b479ee7fe64d998bb40616a0cc7fad1c6eefe5f6a2c0`
- Native PNG SHA-256: `7c786392ab8d0343f9bf24162f198855715e9f5579e2ce89d36cfeacf87668fb`
- Normalized vertex stream SHA-256: `bf09f4f4006a2807584995cecc48f2387a489373190b5a44837250a596ccaaf8`

Compact results are in [pgxp-audit-evidence.json](pgxp-audit-evidence.json).
Local captures and complete receipts are under `analysis/pgxp-audit/`; retail
saves and images are not committed.

## Causes fixed

1. Converting absolute 16.16 screen coordinates to float discarded different
   low bits in the two VRAM pages. Keep double precision until subtracting the
   draw-area origin, then emit local float vertices.
2. Seam indexing and barycentric expansion depended on absolute coordinate
   rounding. Normalize each seam domain and calculate barycentric displacements
   directly. Avoid an identity backdrop transform that needlessly adds and
   subtracts a large origin.
3. Hardware texture/colour interpolation retained tiny absolute-window-origin
   differences. A linked geometry stage carries triangle-local positions and
   reciprocal-weighted attributes; fragment interpolation is local and retains
   perspective correction. Raster positions use 1/256 of an output pixel.
4. Interpolating normalized presentation UVs changed fractional texel weights
   between pages. Resolve in local source pixels and add the integer origin only
   when fetching texels. This preserves bilinear and area filtering.

The previous agent's idempotent GTE configuration/cache-generation fix remains.
Neither precision fallback changes nor alternating depth domains explained this
static scene's A/B images: the accepted guest streams were already identical.

## Counters and lifecycle

Each drawn frame: 2,580 precision lookups, 58 integer fallbacks, 31 value
mismatches, zero truncation/tolerance rejects, and 2,220/2,220 valid depth-word
lookups. The normalized renderer stream contains 62,190 floats with zero changes.
A drawn-frame depth probe recorded 536 matching seam decisions, zero changed
seam decisions, two threshold clears, 625 failed depth samples and zero failures
within normalized epsilon 2^-20. Invalid w and depth input counters stay zero.
The probe is opt-in and synchronous; it must be off for performance measurements.

Disk loads and actual rewind acceptance preserve the final image/stream after
host precision provenance is invalidated and reconstructed. Existing GTE/PGXP
regressions additionally cover raw CPU restore, speculative/checkpoint rollback,
frozen extra-frame contexts and generation changes. The GL fixture uploads a
white texture and a U-gradient texture, then verifies actual perspective colour
and UV values, so a matching black image cannot pass as texture correctness.

## Forced particles: separate pause failure

A separate paused test triggers the existing `r4.effects flame 0 1` debug command.
The backfire counter advances once, and 32 consecutive presents have **15 image
hashes**. Native images and normalized PGXP streams remain identical. All changes
are confined to x=1748..3165, y=2236..2879 in the 5120x2880 output, around the
car's particle region; 566,784 pixels change in the union. The surrounding scene
is byte-identical. This is actual particle animation/expiry, not the fixed
alternation of the two display buffers.

`r4_car_effects.cpp` advances particles from its game-frame/VBlank sample and
currently has no pause guard around `game_events` / `advance`. That title-effect
source is outside the explicitly permitted PGXP/GTE/precision-cache/depth/seam
scope, so it was not changed or masked with a renderer workaround. The strict
paused-image requirement with live particles therefore remains **failed**. The
normal paused test above keeps Car Effects and lighting enabled, but does not
force an active particle burst.

Reproduce this negative check by adding `--particle-car 0` to the audit command.
It requires a real backfire-counter increment and fails if the captures change.
The report records this separately from the passing no-burst stability/replay
checks. Full phase 3b closure requires the supervisor to address this title-effect
pause behavior as well as reconcile the accuracy criterion below.

## Accuracy: explicit unresolved criterion

The audit detects 842 accepted vertices more than one native pixel from integer
GTE results: 324 unclamped projections and 518 preserved projections, zero derived
coordinate cases. These are stable across replay. The raw normalized ring also
includes wide/unclamped coordinates, so it is not interchangeable with the
pre-transform lookup counter.

A minimal analytical example: MAC1=4095, MAC3=151*4096, H=300, OFX=0. Integer
IR1 truncates to zero, hence SXY.x=0. Preserving the MAC fraction gives x approximately 1.98627.
There is no way to keep that exact projection and simultaneously enforce a
one-pixel distance from zero. A regression verifies the exact result. No clipping
or sub-feature disable was added to hide these counters. The literal one-pixel
acceptance criterion therefore remains open; the temporal-stability criterion
passes. This is not a claim that every moving-race visual artifact is eliminated.

## Tests and reproduction

- Five CPU suites pass: PGXP, word-shadow, seam snap, seam batch, GTE register
  access. The new seam test requires bit-identical decisions across translations.
- Hidden real-GL regression: 2,000 seeded triangles of each of four kinds at
  three VRAM origins, scales 1x/4x/9x and a separate 18x run. Zero translated
  pixel differences. Fractional presentation resolve, analytical perspective
  colour/UV, invalid w/depth rejection, uploads and shader-stage linkage pass.
- Calibrated near-depth samples: 2,485 / 39,340 / 198,765 / 794,430 at
  1x/4x/9x/18x. All count as near in the close trial, none in the far trial.
- Full render-thread fixture: 80 frames, thread off/on at 1x/4x plus dynamic
  resolution at 4x. All readback/native/high-resolution/wide digests match;
  ring wrap and ten dynamic-resolution steps pass.

Start the runtime hidden with dummy audio and `PSX_PRESENT_SHOT_BURST=8`. Load
an isolated paused race slot, then run:

```powershell
python tools/pgxp_static_audit.py analysis/pgxp-audit/recheck --port 4898 --samples 2 --expected-burst 8 --replay-slot 1 --replays 2 --require-effects
```

The harness checks the real GL interpolation stage, requires nonempty drawn
streams, compares native images too, and refuses incomplete burst captures.
Use a new output directory. Allow load notifications to expire; the default
settling delay is six seconds. For rewind testing, enable `PSX_REWIND=1`, open
F8 through `host_key`, and navigate/accept using `set_input` with `layer:"host"`.
Clear the input afterwards. The local `run_lifecycle.py` records that sequence.

Framework diagnostic details and the CMake test/compiler option are documented
in [psxrecomp/docs/PGXP_AUDIT.md](../psxrecomp/docs/PGXP_AUDIT.md). The central Beads
path `F:/Software/beads/issues` is unavailable on this host; no local database was
created. No changes were made in the owner's main checkout or its build folder.

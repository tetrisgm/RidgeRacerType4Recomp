# R4 extra-frame qualification — 2026-11-10

## Status

**Not accepted for packaging. Stable 4K / 60 distinct images per second has
not been achieved.** The restored draw-replay feature is off by default and
has no netplay opt-in. The menu, controls, Steam support and prior native-30
qualification are retained; see [the native performance receipt](PERFORMANCE_FIX_20261110.md).

The owner inspected a visible run and reported no tunnel-seam, sky, ground,
misplaced-gauge or frame-order defects in that run, but repeated slowdowns.
This is route-specific observation, not all-scene visual acceptance. A nearby
sample had 54.85 swap completions/s, a 235.370 ms maximum gap and 14 hitches.
Most intermediate redraws were being shed.

## Findings and corrections

- The historical queued redraw path is expensive even with diagnostic captures
  and GPU queries disabled. Actual complete redraw costs must include both CPU
  admission and render-queue admission; the diagnostic bypass originally covered
  only the CPU guard. That measurement error is corrected.
- Increasing the queue from two to four VBlanks produced near-60 swap counters,
  but also rejected most intermediate redraws. These results are **not** evidence
  of near-60 distinct images. For example, `ordered-clean-cost` completed only
  520 of 1,646 wanted passes; 1,126 were rejected by queue admission.
- With both admission guards bypassed for attribution, `full-admission-cost`
  completed all 571 wanted passes, with zero backlog shedding, at only 34.21
  VBlanks/s. The subsequent frame-notification change measured 35.03 VBlanks/s.
  Neither result meets full game speed. No resolution, PGXP or detail reduction
  was used: fixed 9x / 2160 lines, 21:9 (5040x2160 internal image), Max Detail.
- Generic stale-present suppression skipped temporal flip tracking. Queued
  temporal presentation now observes every VBlank; its output selector owns
  phase selection. A saturated real-GL test checks all 120 outputs.
- Transactions omitted depth attachments and PGXP depth-group state from
  restoration. Both are now restored, including native-wide surfaces.
- Temporal native captures did not resolve post-effect mask/depth provenance
  before source reuse. The restored provenance step is checked against the
  ordinary native mask, including HUD occlusion.
- Pending phase-zero copies and repeated native-wide center copies were
  redundant. Promotion captures the final native image, including late HUD writes.
- Queued vertex uploads can be reused without changing draw order, geometry or
  texture state. Exact reference hashes match at 1x/4x, including dynamic scale.
- A producer waiting for frame completion was woken by individual draw records
  that could not satisfy its wait. It now receives frame-marker notifications;
  drain, ring-space and context handoff waits retain their notifications.

Shader coverage/fusion, state-query removal, copy reduction, vertex-upload reuse,
a larger queue and a separate presentation thread did **not** establish stable
60 distinct frames. Do not present their combined swap counters as that result.
The Apple GPU elapsed-query probe returned zero for blit-only sections; it does
not establish that backup/restore copies are free. The temporary probe was removed.

## Validation and evidence

Local source harnesses, binaries and raw receipts are under
`analysis/60fps-20261110/` (not tracked game data):

- `visible-review/race-start-observation.json`: visible-run stall telemetry.
- `full-admission-cost/`, `frame-wake-cost/`: actual complete redraw cost.
- `ordered-clean-cost/`, `ordered-default-cost/`: rejected near-60 swap claims.
- `queue-upload-fixtures-retry/`, `queue-upload-reference/`: six real-GL
  thread/scale modes pass with matching native, wide and readback hashes.
- `depth-capture-parity/`: 132 checks, zero failures, actual scales 1x/4x/9x;
  depth/color/stencil restoration and final phase-zero pixels.
- `bloom-temporal-fixture/`: 187 checks, zero failures, including exact temporal
  mask equality and no HUD contamination in the fixtures.
- Render-thread core: 8,843 checks, zero failures after frame-only notifications.
- `checkpoint-hd/`: HD replacement fixture passes (219 checks at 4x), including
  snapshot and native-wide coverage.
- `checkpoint-native-cost/`: 120.47-second moving 4K race, 60.005 VBlanks/s,
  29.92–30.07 native images/s across five-second samples, zero dispatch misses
  and zero extra passes. This preserves native cadence; it is not 60 distinct.
- `checkpoint-menu-smoke/`: full race/menu smoke, Select/Back, controls, settings,
  save/load and restart pass, with zero dispatch misses.
- Release build and all 33 CTests pass. Render-pass sandbox, abort and static
  guards pass, including intentional fault rollback.

Local engine checkpoint: `5846d90c` (queued rendering) following `9ac07698`
(frame-bound wakeups). Neither checkpoint is published as a qualified release.

Earlier `fullres-motion` captures raised an ordering concern. It still needs a
new consecutive full-resolution route review after the temporal scheduling fix;
unit/fixture success does not establish that tunnel and sky gameplay are clean.

## Next architecture decision

MStan's `c7ebe109`–`1b84d4fd` lineage retires guest redraw and inferred matching
in favor of explicit scene identity, transforms and immutable texture versions,
rendered into a separate target. Its documented pilot is Ape Escape at 3x,
not R4 at 4K. It needs an R4 producer plus HD-texture, native-wide, post-effect,
controller-latency and full-resolution visual qualification. It is not a drop-in
performance patch and has not been imported here. No nearest-position matching,
PGXP-off reference, lower-resolution acceptance or duplicate-frame success is
an acceptable substitute for that work.

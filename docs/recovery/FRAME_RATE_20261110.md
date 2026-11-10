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

## Continuation notes for Claude

### Start and scope

- Game `428fb4ec`; engine `5846d90c` after `9ac07698`; UI `ef8150f5`.
  These game/engine checkpoints are local and unpushed. Previous qualified
  native baseline: game `46c0c3ec`, engine `9fbb6ba8`, same UI.
- Read `docs/recovery/HISTORY_AUDIT_20261109.md` and
  `docs/recovery/THREADING_EVALUATION_20261109.md` before choosing further
  historical ports. Branch cleanup/history preservation is already recorded;
  do not restart consolidation or use the NAS. Work only on the Mac.
- Read engine `CLAUDE.md`, `docs/internal/PLAN.md` and
  `docs/internal/FAITHFUL_TIMING_PLAN.md` before engine edits.
- Retain PGXP, 9x / 2160 lines, 21:9, Max Detail, effects and rewind when
  comparing the recorded runs. Owner's present-thread setting is false.
  Do not count repeated swaps as distinct frames or reduce quality to pass.
- Automated tests use isolated saves/settings, hidden windows and dummy audio.
  Owner also explicitly requested visible verification: skip the intro and load
  the starting-grid save. Never reopen a game the owner closes. Serialize builds
  and benchmark games, use two compile jobs, terminate only owned processes.

### Reproduction traps

The feature was disabled by default **after** the earlier benchmark harnesses
were written. `analysis/60fps-20261110/frame-wake-cost.py` and
`full-admission-cost.py` now need an explicit feature entry in their isolated
`mods/state.toml`, in addition to the existing internal-resolution entry:

```toml
[[package]]
id = "r4.enhancement.frame-rate"
[[feature]]
package_id = "r4.enhancement.frame-rate"
id = "frame-rate"
enabled = true
[feature.values]
rate = "60"
```

Use a fresh output directory; the harnesses create their directories and refuse
existing ones. Assert `render_pass_stats` has actual completed passes before
interpreting a 60-mode result. `PSX_RENDER_PASS_SHED=0` now bypasses both CPU
and render-queue admission in debug builds, for full-cost attribution only.
Normal admission still sheds at a two-frame backlog even with a four-frame
queue; this explains many misleading near-60 swap results. The off-by-default
feature has no netplay opt-in and its local-camera helpers are not connected
to the current own-view bridge.

`checkpoint-native-cost.py` is deliberately native-only. Starting-grid saves
are in `analysis/mac-performance-20261109/grid-source/grid/memcards`. Harnesses
stage their own copies, use port 4799, load slot 0 and run
`tools/perf/autopilot.py`. Verify that the car/camera actually moves: some older
endpoint captures were stationary or taken before the starting lights changed.

### Next engineering decision and acceptance

The guest-redraw path has not met the target after the recorded optimizations.
Evaluate the explicit-scene, separate-target approach in engine history
`c7ebe109` through `1b84d4fd`; do not blindly cherry-pick its legacy retirement.
Relevant historical files are `presentation_scene.h/.c`, `presentation_draw.h/.c`,
`presentation_gpu.h/.inc`, and `gte_presentation.h`. The Ape pilot is not an R4
producer. R4 needs stable scene identity, transforms, texture versions and
compatibility with HD replacements, native-wide geometry and post-effects.
This is a candidate architecture, not a proven solution or a completed port.

Visual acceptance must include race start, blue sky before the tunnel, tunnel
seams, ground textures and gauge placement, with consecutive full-resolution
frames checked for order and blinking. The owner's positive observation was
for one visible run with most extra frames shed. The earlier `fullres-motion`
capture raised an order concern; it was not re-qualified after scheduling fixes.

For capture, set `PSX_PRESENT_SHOT_BURST`, record `present_shot_seq`, request
`present_shot`, and wait for sequence advancement **and** `wrote=1`. Do not
stop when file count alone reaches the target: the last PNG can still be
partial (this happened in the 30-frame capture). `render_pass_dump` records
native/generated pairs; require complete PNG IEND trailers. Capture/readback
cost must be excluded from performance measurements. Small contact sheets
cannot establish that fine polygon seams are clean.

Keep distinct-image rate, game/VBlank cadence, completed/rejected passes,
frame-time spikes and input latency separate. Full-resolution visual success
and sustained performance are both required before packaging. Physical Steam
Controller, visible audio/pacing, broader courses and multiplayer remain
unqualified; automated menu tests do not establish those results.

### Verification and delivery

Latest checks passed before checkpoint: Release build, 33 CTests, full menu/race
smoke, render-pass guards/sandbox/abort, render-thread core, and real-GL depth,
bloom, upload-reference and HD fixtures (receipts listed above). Supported
`JOBS=2 tools/regen.sh` reused 7,459 audited variants with no generated-C change.
Regenerate again after future engine pin changes.

```sh
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
python3 tools/gen_r4_interp_fields.py --check
python3 psxrecomp/runtime/tests/test_render_pass_guards.py
python3 tools/verify_native_overlays.py analysis/NEW-UNIQUE-DIRECTORY --menu-script tests/custom_menu.txt
```

The overlay verifier includes the full smoke sequence. Real-GL fixture runner:
`psxrecomp/runtime/tests/run_gl_texture_filter.py`; threading runner and stored
harnesses/receipts live under the paths listed above. All raw game captures,
saves and staged binaries remain ignored/local. Do not commit game data.

Do not publish the checkpoint as a 60 FPS release. Once the target passes,
update pin history/qualification, commit and push coherent verified changes,
and follow `docs/WINDOWS_BUILD.md` for a new unique portable package built on
the Mac. The existing `dist/windows/2026-11-10-4k-performance-01/` predates this
experiment and is a native-30 candidate, not a current 60 FPS delivery.

## Completion-fenced attribution and private-target probe

A subsequent diagnostic brackets backup, draw, capture and restoration with
`glFinish` and the host monotonic clock. It drains pending native batches and
wide mirrors before starting. This deliberately serializes GPU work: the
results attribute completed work, **not** asynchronous throughput or pure GPU
execution time. Draw includes render-owner command consumption and any producer
starvation. Source receipt: [stage costs](FRAME_STAGE_COST_20261110.json).

| Probe | Backup ms | Draw ms | Capture ms | Restore ms | VBlanks/s |
|---|---:|---:|---:|---:|---:|
| Existing transaction | 9.193 | 17.850 | 1.884 | 6.865 | 34.64 |
| Separate color/depth blits | 9.057 | 17.508 | 1.809 | 6.634 | 35.41 |
| Complete private target | 18.410 | 17.719 | 1.897 | 1.102 | 33.72 |

All runs retain fixed 9x, PGXP, full detail and explicit extra-frame enablement,
with admission shedding bypassed to expose cost. The old CPU-only transaction
timer reports roughly 7.4 ms guest work; it does not account for GPU completion.
The private-target probe copies complete native surfaces into separate storage,
draws there and restores by switching back to untouched original surfaces. It
preserves arbitrary texture-feedback source contents but copies too much. Its
restoration savings were outweighed by initialization. Neither attempted change
was retained in runtime behavior. The prototype patch and isolated binaries
remain locally under `analysis/60fps-20261110/`.

The evidence supports eliminating framebuffer cloning, not merely moving it to
another target. A replacement should consume explicit R4 scene geometry, camera,
object transforms and immutable materials; render into its own output; and keep
original game logic, menu and controllers. It must establish exact native-endpoint
parity before interpolation. The R4 producer and replacement remain unimplemented;
these probes are not a performance fix or release qualification.

The opt-in diagnostic is `PSX_RENDER_PASS_STAGE_TIME=1`, queried using
`render_pass_stage_times`. Its four total/count entries are backup, draw, capture,
restore in that order. Default behavior has no completion fences. Reproduction:
`analysis/60fps-20261110/completion-fenced-drained-cost.py`.

### Visible reference capture

`analysis/60fps-20261110/measured-visible-baseline/` loaded the saved race directly
and drove five route samples with extra passes fully admitted, not mostly shed.
Each sample has four complete native/generated pairs at 5040x2160, plus the
1280x720 composed visible-window screenshot. The receipt confirms fixed 9x and
zero dispatch misses. This was a visual-reference run, not a throughput benchmark
(captures introduce stalls). The owned game process exited after capture.

Inspected overview screenshots include sky/road, a green tunnel and the orange
tunnel. A full-resolution green-tunnel wall crop across consecutive native and
generated pairs shows continuous wall/road edges and gauge placement; no open
seams or misplaced gauge were observed in that inspected subset. Additional
full-resolution crop sequences of the orange-tunnel ground and the sky/bridge
edge also show no missing patches in the inspected pairs. This is not
continuous-video coverage, proof against all blinking, or acceptance of a new
renderer. No new renderer was visually qualified.

After retaining only the opt-in diagnostic: Release build, all 33 CTests and
`stage-final-menu-smoke/native-overlay-check.json` pass (zero dispatch misses).
Menu, Select/Back, settings and controller integration are unchanged.

# BRIEF-4 results

Stage 1 accepted on `fix/slowdowns` in both repositories. Production Release,
debug tools off, fullscreen 7680x4320 at 60 Hz, default Ultra, recorded route,
slot 2. Completion clock throughout; measurements start at route frame 900.

| Stage 1 change | Load | Completion Hz | Gaps >25 ms | Max gap ms | p99 emulation busy |
|---|---|---:|---:|---:|---:|
| Before resolution cap (27x / HiW) | quiet | 46.671 | 814 | 63.131 | 47.7% |
| Before resolution cap (27x / HiW) | 23.62% host CPU | 45.205 | 816 | 60.388 | 39.4% |
| 8K cap + SSAA threshold (18x, full VRAM) | quiet | 60.000 | 6 | 39.739 | 45.1% |
| 8K cap + SSAA threshold (18x, full VRAM) | 23.36% host CPU | 60.000 | 7 | 46.054 | 51.5% |
| Remove PGXP depth GL state queries | quiet | 60.001 | 8 | 70.377 | 53.2% |
| Exclude host waits from GPU query spans | quiet | 60.000 | 9 | 72.229 | 48.5% |
| Exclude host waits from GPU query spans | 23.44% host CPU | 59.998 | 5 | 52.780 | 60.6% |
| Exclude presenter pacing from dynres backpressure | quiet | 60.020 | 4 | 75.059 | 48.0% |
| Exclude presenter pacing from dynres backpressure | 23.34% host CPU | 60.009 | 4 | 72.844 | 50.0% |
| Reuse storage + corrected pacing accounting | quiet | 59.990 | 0 | 20.775 | 44.6% |
| Reuse storage + corrected pacing accounting | 23.51% host CPU | 59.956 | 0 | 21.173 | 48.8% |
| Repeat after final cap policy checks | quiet | 60.004 | 0 | 19.732 | 46.4% |
| Repeat after final cap policy checks | 23.35% host CPU | 59.957 | 3 | 66.734 | 55.4% |
| Event-published drawable dimensions | quiet | 59.856 | 36 | 105.705 | 70.6% |
| Event-published drawable dimensions | 23.28% host CPU | 59.990 | 0 | 22.035 | 55.9% |
| Remove production PGXP diagnostic copies | quiet | 59.990 | 2 | 38.060 | 50.5% |
| Coverage-only filtering + independent sleep timers | quiet | 59.973 | 0 | 20.266 | 46.1% |

Storage-reuse experiment: 59.940 Hz, zero gaps >25 ms, 21.836 ms maximum,
45.6% p99 busy. This isolated experiment fell to 4x under the old presenter
accounting. With corrected accounting, both later runs stay at 17x..18x.
Patch and receipts remain local under `analysis/brief4/local/dyn-storage*`.

Receipts: local `analysis/brief4/local/baseline/*-native-summary.json`, matching
CSV/stdout and load receipt. Native overlay calls positive, interpreted calls zero.
`state-proof.json` proves the master-compatible private slot and prescribed
`production-counter-saves` slot have identical bytes except header offsets 32..35;
the guest/device payload SHA-256 is
`fff644dbaf37984be1066170131c2d887f211af8220ac5b3cd50e7de8eefd1a4`.
Cap receipts: local `analysis/brief4/local/cap/{quiet,loaded}*`. Original saves and player settings remain untouched.

Accepted evidence: `coverage-production/quiet-summary.json` and
`drawable-events/loaded-summary.json` under local `analysis/brief4/local/`.
Rates round to 60.0 Hz; zero gaps over 25 ms, p99 busy below 60% in both.
The loaded case passed before the final production-only PGXP, coverage and
Windows timer fixes; only failing quiet was repeated, per the owner rule.
All runs have native overlay calls, zero interpreted/dirty execution and no
dispatch misses. Title ctest: 21/21 pass (`stage1-title-tests.txt`).
50 generated shards unchanged (`coverage-regen.txt`). The final binary uses the
prescribed production-counter save directly; `pgxp-production/state-proof.json`
confirms the same guest/device payload. Prior Windows/Mac component checks are
archived locally; no extra experiments were run after the minimum-testing note.

## Stage 2

Owner decision: exactly **40 sections ahead**, one behind, in Maximum. No
ceiling search continues. Stage 1 settings, full-depth PGXP and conservative
pre-GTE visibility rejection remain. The fixed default does not step down
under load. Course RAM is 8 MB; the private prescribed slot zero-extends RAM
without changing the original guest/device payload.

The post-tunnel turn is route frame 2400. Master has 60 consecutive fullscreen
frames with unchanged distance, plus the requested PGXP-off/widescreen-off
controls. The gross skyline shift survives both controls. The correction
preserves positive unwrapped MAC3 through validated PGXP dataflow and GPU
perspective/depth encoding; guest GTE registers and legacy u16 depth APIs stay
unchanged. The rejected 65,535 far-plane cull hid visible towers and never
qualified as a pass. Full depth restores their correct far projection.

| Stage 2 change | Load | Completion Hz | Gaps >25 ms | Max gap ms | p99 busy |
|---|---|---:|---:|---:|---:|
| Full depth, full circuit, flat fine culling | quiet | 59.874 | 16 | 30.951 | 91.7% |
| Same | moderate CPU | 54.929 | 287 | 40.882 | 80.8% |
| Owner-fixed 40 ahead | quiet | 60.000 | 0 | 22.851 | 56.0% |
| Same | 23.39% host CPU | 60.000 | 1 | 25.641 | 61.5% |
| Submit before pacing (reverted) | moderate CPU | 60.000 | 8 | 31.057 | 71.4% |
| Match Windows producer priority | moderate CPU | 60.000 | 0 | 24.455 | 61.6% |
| Inherit proven planes (reverted) | moderate CPU | 60.003 | 2 | 26.242 | 73.7% |
| Bulk queued uploads + matched priority | moderate CPU | 60.000 | 0 | 23.013 | 58.7% |

Accepted timing receipts (local `analysis/brief4/local/`):
`stage2-fixed40/quiet-*` and `stage2-fixed40-upload/loaded-*`. Only failing
loaded was repeated after cost fixes, per the minimum-testing rule. Both have
positive native overlay counts (69,996 / 69,998), zero interpreted/dirty
execution and no dispatch/segment misses. Windows and isolated Mac title ctest
pass 21/21 (`stage2-fixed40-upload-title-tests.txt`). Mac production PGXP
numeric checks also pass. Bulk uploads reuse `910eef11` without its unrelated
coverage changes; failed pacing and plane-traversal experiments are removed.

Visual receipts: `master-corner-2400/` (before and controls),
`stage2-facing-turn-2400/` (full-depth after; 60 consecutive native frames),
and `stage2-fixed40-turn-2400/` (final 40-section after: frames 2398..2457,
60 consecutive, 38,404 native calls, zero interpreted calls/misses). The hierarchy and
backface-culling captures were pixel-identical at the first/last frames.
Late grey-roof tracking was occluded by a foreground pole; its correlation
jump is not building drift evidence.

Stage 2 accepted; final pin: framework `8130e6fd`. Regeneration leaves all
50 game shards unchanged. Stage 3 remains next. No PR, merge or
release is authorized. See the owner brief for the ordered acceptance gates.

## Stage 3 (in progress; not accepted)

BRIEF-5 runtime checkpoint: framework `a54b9f00` / `2bedbe9a` adds private memory
backing, page-versioned PGXP/GTE precision snapshots, a thread-local frozen
clock, and GPU state selection across 323 fixed regions. Concurrent memory,
precision, GPU and clock checks pass on Windows and Mac. Actual-GPU GP0 history
and upload-retention checks pass on both; the Apple fixture linker was fixed
to avoid replacing tentative fixture globals with abort functions. The GPU
region manifest/proof is tracked at framework
`docs/internal/receipts/brief5-gpu-isolation.json`. DMA/dispatch and the replay
worker remain in progress; no new route gate has run, and branches are unpushed.

Earlier prerequisite: framework `8d73618f` / `58f17410` makes overlay callback
tables, pending cycle charges and host dispatch pointers thread-local. Each
executing thread must bind each shard with `overlay_init`; rebinding publishes
pending charges through the previous binding. The concurrent production-shim
test, store-PC forwarding, memory-address forwarding and shim compilation
pass on Windows and macOS. Windows title ctest passes 25/25. OpenBIOS
regeneration updates only its emitter fingerprint; all 50 R4 shards remain
unchanged. This is a prerequisite only: private RAM/GTE/PGXP/GPU/DMA/dispatch
contexts, snapshot publication and replay-worker execution are not implemented.
No new fingerprint, performance or cadence acceptance runs; branches unpushed.

`feat/render-60fps-v2` imports transactional interpolation and HD/local-view
admission over accepted Stages 1?2. R4 redraw now preserves Maximum's expanded
primitive-heap layout; using the old stock heap caused watchdog rollbacks.
Fullscreen, native-cadence identity: 4,501 off/on fingerprints match exactly,
with 0 / 735 completed temporal passes and no on-path aborts/discards.
Local receipts: `stage3-identity-native-{off,on}/identity.json` and
`stage3-identity-native-compare.json`. An earlier mislabeled off setup carried
a stale 120 Hz override and is rejected. Its watchdog logs led to the heap fix.

| Stage 3 change | Load | Completion Hz | Gaps >25 ms | Max gap ms | p99 busy |
|---|---|---:|---:|---:|---:|
| Imported plugin (render thread disabled; no passes) | quiet | 59.845 | 38 | 75.212 | 100.0% |
| Async eligibility + expanded heaps (749 passes) | quiet | 59.930 | 451 | 35.311 | 92.8% |
| Queued deadlines (393 passes) | quiet | 60.093 | 422 | 42.187 | 95.6% |
| Release refused plans (8K slot budget rejects passes) | quiet | 60.000 | 8 | 30.177 | 63.0% |
| Size-aware 8K image budget (456 passes) | quiet | 58.528 | 311 | 63.434 | 65.0% |
| Recorded geometry redraw (rejected) | quiet | 60.000 | 14 | 84.177 | 66.1% |

The retained transactional implementation permits two images per generation at
8K, up to a 1 GiB image-queue budget. Full-resolution temporal execution still
acquires the one GL context synchronously: 2,091 acquisitions cost 33.8 s in
the 75 s replay (`stage3-8k-generations/quiet.csv.renderer.json`). Most requested
passes were shed (456 completed / 2,090 wanted). It cannot qualify as distinct
60 fps. The next structural work is queued GPU transaction begin/capture/restore,
so guest-state restoration does not wait for the presentation queue.

The alternative existing recorded-geometry generator also fails: 1,070 generated
images / 2,255 game flips, with no image reprojection, plus the timing failure
above. Its startup selector was removed; patch and executable receipts remain
under local `stage3-geometry-redraw/`. No hidden switch selects it in this branch.

All listed production runs have positive native overlays, zero interpreted/dirty
execution and no dispatch/segment misses (`quiet-native-proof.json` in each
receipt directory). The 4,501-frame identity result predates the latest host
queue changes; it proves the expanded-heap transaction path, not final Stage 3
acceptance. Final timing/distinct-frame gates remain open; loaded was not run
while quiet fails. Windows/Mac title tests and interpolation schedule/guards
pass. Stage 3 work is committed locally, unaccepted and unpushed. No PR, merge or
release. Stage 2 remains accepted and pushed.

### Stage 3 owner-direction follow-up (unaccepted)

Temporal GPU begin/draw/capture/restore is queued on the GL-owning render
thread. Context acquires fall to one startup `sync_cpu`. The asynchronous
CPU-sandbox path still matches 4,501 off/on fingerprints (0 / 1,224 passes):
`stage3-identity-render-owned-{off,on}/identity.json`,
`stage3-identity-render-owned-compare.json`.

| Follow-up | Completion Hz | Gaps >25 ms | Max gap ms | p99 busy |
|---|---:|---:|---:|---:|
| Queued transactions, initial resources | 58.194 | 7 | 1801.230 | 71.2% |
| Retained resources, broader CPU admission | 60.000 | 8 | 75.540 | 98.8% |
| First dataflow build (activation refused before GL init; rejected) | 59.997 | 214 | 62.879 | 97.1% |
| Dataflow GPU redraw + nonblocking present queue | 60.006 | 3 | 25.639 | 70.1% |

Corresponding local directories: `stage3-render-owned`, `stage3-retained-ready`,
`stage3-dataflow-rendered`, `stage3-dataflow-nonblocking`. Native counters remain
positive with no interpreted/dirty instructions or dispatch misses.

The current dataflow path redraws actual recorded geometry in a display-rect
GPU transaction. Value-validated PGXP word copies carry model identities and
signed full-depth homogeneous positions; matching is per view, with no fitted
camera or image warp. The last run completes 2,225 GPU redraws, matching 6,920 /
6,990 projected vertices in its final generation. Presenter queue waits are
zero, versus 31.4 s before nonblocking submission. Physical cadence still
fails: the last 128 completed swaps are only 19.54 Hz; most extras are dropped.
Producer wall-clock PTS now replaces replay-time clock advancement, and input
origin metadata is restricted to the six GTE model words rather than ordinary
integer instructions. These corrections are not yet qualified. Loaded remains
pending. Stage 2's fixed 40 sections and full-depth projection fix remain intact.

Windows geometry, production PGXP, transaction sandbox/abort and presenter
ownership tests pass. The Mac dataflow build/title check passes 25/25 before
the newest producer-clock correction. No Stage 3 push or acceptance.


### BRIEF-5 private execution continuation (unaccepted)

Framework `efe800c9` adds private IRQ/DMA/timer/I-cache and plugin callback
ownership, captured code-validity/MMIO metadata, native code lookup in generated
game/BIOS dispatch, and bounded immutable command capture independent of the
live render-ring producer. Memory/IRQ concurrency and capture/consumer/overflow
fixtures pass on Windows and Mac; Mac syntax checks cover the changed devices,
mod runtime and regenerated OpenBIOS. This is an implementation checkpoint,
not a blocker or gate result. Worker/plugin-host-state binding and GL handoff
remain in progress. No final route fingerprint, quiet, loaded or distinct-frame
gate has run for this implementation; neither branch is pushed.


### BRIEF-5 worker integration checkpoint (unaccepted)

Framework `6ae7f390` adds the double-buffered native guest draw worker, sealed
native dispatch, per-plugin host-state images, captured GPU/GTE frame provenance,
and nonblocking immutable-command handoff on the existing render queue. R4 now
publishes owned interpolation endpoints instead of selecting recorded GPU redraw.
Activation changes quiesce the worker before changing hook tables. Windows five
focused checks pass (GTE reference/enhanced, dispatch, plugin state, capture);
Mac dispatch/plugin/memory/capture and GTE reference checks pass. GPU snapshot
concurrency passes O0/O2 on both hosts. Mac syntax checks cover the worker and
changed title plugins. Release integration links before the final lifecycle
edits and is being rebuilt. Native SDK/cache refresh, integrated verification,
and ordered route identity/quiet/loaded/distinct-frame gates remain pending.
This is continuing implementation, not a blocker or acceptance. No push.


BRIEF-5 qualification checkpoint `87270c7e`: native cache refresh builds 14
modules with zero failures. Production off/on replay fingerprints match all
4,511 collected rows (CPU/clock/timers/RAM/dirty/scratch/DMA/SIO/CD/SPU/MDEC/GPU).
Both source paths have positive native overlay calls, zero interpretation and
zero dispatch/segment misses; worker lookup also has zero misses. This gate is
still FAILED: two jobs remained unconsumed, producing zero extras, and shutdown
terminated with a joinable worker. The present handoff and shutdown fixes are
committed and rebuilding; repeat identity before quiet/loaded/cadence. Evidence:
local `brief5-identity-off-v2`, `brief5-identity-on`, `brief5-native-compile-fixed.txt`.
Fixture migration changes only host header IDs; the guest payload hash is
unchanged and linked to the original counter save by the Stage 2 proof. No push.

BRIEF-5 follow-up: corrected present handoff passes production identity: 4,511
matching rows, 1,423 private draws, clean exit, zero interpreted/dirty execution
or source/worker native misses. The first quiet gate fails (60.000 Hz,
6 completion gaps >25 ms, 79.632 ms maximum, p99 busy 68.9%): local
`brief5-worker-production/quiet-summary.json`. Loaded and cadence remain pending.
Framework `fb28d7c0` retains temporal history storage across resolution reductions,
permits unfinished jobs through their duplicate-VBlank deadline, and supplies
persistent zeroed plugin publication storage. R4 versions its large private
course-cache copies. Sampling and receipts: local `brief5-worker-production`;
updated qualification: `brief5-worker-retained`. Windows/Mac plugin snapshot
checks and Release runtime builds pass. Continuing qualification, no blocker,
acceptance or push.

BRIEF-5 rendering-cost checkpoint `56a8b24f`: composing-owner post-effect state
tracking removes a sampled driver round trip. Private GPU backup/capture cost
now enters dynamic-resolution timing, and a readiness-only fence bounds optional
GPU work. Windows/Mac GL restoration/equivalence/dynres checks and Release builds
pass. Per-cache R4 publication revisions retain 4,511 identical production rows
with positive native execution and zero interpretation, dirty instructions or
source/worker lookup misses. Latest quiet still fails: 59.802 Hz, 1 gap >25 ms
(max 25.018), p99 busy 100%. Loaded/cadence are pending. Source-thread sampling
now attributes remaining work primarily to cycle/PGXP execution, with only three
samples in the R4 cache capture. Continuing cost fixes; no true blocker or push.
Receipts: local `brief5-worker-admission` and `brief5-gpu-accounting-check`.

BRIEF-5 source-cost checkpoint `8114565a`: read-only PGXP lookup tracking and
publication epochs preserve identity (4,511 rows) and quiet now reaches 60.000 Hz
with 0 gaps >25 ms (max 24.536 ms), but p99 busy remains 100%; still FAILED.
Receipts: local `brief5-worker-readonly`. GPU admission/drop remains nonblocking.
Visible-fullscreen sampling identifies cycle helpers; a small inline one-cycle
path is now checked against the complete charge/interlock path across 65,536
steps and scale/gate modes, with assertions enabled on Windows/Mac. Independent
PGXP snapshot/validation and GTE corpus checks also pass on both. Release/native
refresh and ordered qualification continue; no blocker, loaded/cadence or push.

BRIEF-5 PC-only resume: Release/native refresh passes. Cycle candidate off/on
fingerprints match all 4,511 rows in the same executable, with zero native
misses or interpretation. The older build differs with interpolation off too;
its receipt is not a cross-build equivalence proof. Quiet still FAILS: 60.000 Hz,
0 gaps >25 ms (max 22.247), p99 busy 81.1%. Receipts: local
`brief5-worker-fast-cycle`. Default/stock/high-rate menu choices are implemented
locally but remain unqualified. Source sampling continues; loaded/cadence and
push remain pending. No true blocker. Per the brief redirect, no further Mac
builds or verification are used for this stage.

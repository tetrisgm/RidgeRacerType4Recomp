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

## Stage 3 / BRIEF-5 (in progress; not accepted)

Both `feat/render-60fps-v2` branches remain local and unpushed. Continue until
identity, quiet, loaded and distinct-60-Hz gates pass, then PC title ctest and
push framework before title. PC-only per [BRIEF-5](BRIEF-5.md)'s resume redirect.
Blocked on the common loaded baseline versus the setting-1-only scope; see the
true-blocker paragraph at the end. No PR, merge or release is authorized.

The dedicated worker executes native guest draw code on private CPU/clock,
RAM/scratch/mod memory, GTE/PGXP, GPU/DMA and dispatch state. The emulation owner
publishes snapshots; the GL owner consumes immutable command lists. Optional
work uses readiness checks and is dropped when late. Recorded redraw is off.
The menu exposes stock 30, default 60, and higher best-effort targets including
120/150. Default 60 remains unqualified until full distinct cadence passes.

All timings use production Release without debug tools, fullscreen
7680x4320@60, owner defaults, exactly 40 ahead + 1 behind, slot 2 and the recorded
route. Existing completion-clock summary uses the 60 seconds after frame 900.
Local receipts below are under `analysis/brief4/local/`.

| Worker change / receipt | Load | Completion Hz | Gaps >25 ms | Max gap ms | p99 busy |
|---|---|---:|---:|---:|---:|
| Initial working handoff / `brief5-worker-production` | quiet | 60.000 | 6 | 79.632 | 68.9% |
| Retained textures/cache revisions / `brief5-worker-retained` | quiet | 59.296 | 5 | 374 | 75.7% |
| GL query removal before cost accounting / `brief5-worker-postfx` | quiet | 56.785 | 58 | 2394 | 68.6% |
| Accounted/bounded optional GPU work / `brief5-worker-admission` | quiet | 59.802 | 1 | 25.018 | 100.0% |
| Read-only PGXP lookup/epochs / `brief5-worker-readonly` | quiet | 60.000 | 0 | 24.536 | 100.0% |
| Inline cycle step / `brief5-worker-fast-cycle` | quiet | 60.000 | 0 | 22.247 | 81.1% |
| Forced full-load expansion (removed) / `brief5-worker-load-cycle` | quiet | 60.000 | 0 | 24.826 | 92.1% |
| PGXP operand copies / `brief5-worker-operands` | quiet | 60.001 | 2 | 212.257 | 85.6% |
| Shared clock body / `brief5-worker-shared-clock` | quiet | 60.002 | 0 | 24.519 | 100.0% |
| Publication-only / `brief5-worker-publication` | quiet | 60.000 | 1 | 31.474 | 100.0% |
| Worker-prepared images / `brief5-worker-prepared-native` | quiet | 60.000 | 0 | 19.545 | 69.4% |
| Thread-owned overlay callbacks / `brief5-worker-callbacks` | quiet | 60.000 | 0 | 18.093 | 64.0% |
| GPU scratch + smaller PGXP pages (page change removed) / `brief5-worker-footprint` | quiet | 60.000 | 0 | 21.354 | 100.0% |
| Original pages + publication spans / `brief5-worker-publication-spans` | quiet | 60.000 | 1 | 29.901 | 64.7% |
| Idle-budget replay / `brief5-worker-idle` | quiet | 60.000 | 0 | 16.842 | **58.4% PASS** |
| Idle-budget replay / `brief5-worker-idle` | 22.90% host CPU | 60.000 | 1 | 79.635 | **92.6% FAIL** |

Current original-off identity passes all 4,511 rows: CPU, clock/IRQ, timers,
RAM/dirty/scratch, DMA/SIO/CD/SPU/MDEC/GPU. Positive native source and worker
execution; zero interpretation, dirty instructions and source/worker misses.
Evidence: `brief5-worker-publication/identity-compare.json`. General timing/ALU
experiments were removed; this build matches the original off receipt. Host-only fixture header migrations preserve guest payload SHA-256
`6edce3ec0caea3fc7cb83c5cbaf40e7ff9389c40319e747965f946f6e3903410`.
The original production counter save is unchanged.

Quiet still FAILS. Loaded and distinct cadence have not run while quiet fails.
The latest evening direction stops general hot-path optimization. One source
profile per setting over the same route window (~1500..2400) has identical
native source calls. The setting-1-only stack is async snapshot publication
(~1.8% of samples), primarily the full 8 MiB RAM copy. Sampled mean source CPU
was 50.2% off / 44.2% on; sampled timing is not an acceptance result and does
not establish the asserted 36-point added CPU workload. Receipts:
`brief5-setting-profile-diff/{source,profile-analysis,leaf-diff,inclusive-diff}.json`.

Framework checkpoint `41f3f647` removes the unqualified general cycle/ALU
experiments, retains worker publication tracking, and uses streaming stores
only for RAM snapshot publication on SSE2. Other targets retain memcpy. This
avoids filling the source owner's caches with the worker image without adding
guest-store tracking or changing the copied bytes. O0/O2 concurrent full-RAM
and snapshot-reuse checks pass, as do relevant PC cycle/PGXP checks. Production
refresh and ordered qualification continue. Final PC title ctest is pending.

Prior implementation/checkpoint detail is archived in
[the preceding status revision](https://github.com/tetrisgm/RidgeRacerType4Recomp/blob/02090f8/analysis/STATUS.md)
(local commit until push). Framework reasons and focused receipts are linked
from [pin history](../docs/framework_pin_history.md). Earlier Mac checks predate
the PC-only redirect and are not being continued for this stage.

Framework `e1662e49` moves private slot allocation and page commitment onto
worker bootstrap. Extras are refused until both slots are ready; slot selection
alternates to avoid a late first use. PGXP/GTE no longer allocate private word
images during source capture. The preceding quiet gap coincides with 71,712
page faults. PC cross-thread PGXP/GTE/plugin creation, private GPU, command
capture and O0/O2 RAM checks pass. Production refresh and ordered gates continue.

Worker preparation passes original-off identity (4,511 rows) with native source
and worker execution, zero interpretation/dirty instructions/misses. Quiet now
has no >25 ms gaps and p99 69.4%; loaded/cadence remain pending. The wrongly
nested cache attempt is explicitly invalid in `brief5-worker-prepared/INVALID.json`.

A late-window profile comparison (3450..4350) completed setting 0 natively but
setting 1 exposed a reproducible startup access violation. GDB proves the
source shard writes via an expired initializer-thread TLS breadcrumb pointer
(`brief5-startup-gdb-detail.txt`). Framework `2ef58c54` gives each thread its
own host callback table and binds source DLL TLS on the executing thread.
Production-loader O0/O2 cross-thread lifetime, shim binding and pre-init checks
pass. Production refresh and ordered qualification continue. No true blocker.

Callback fix passes original-off identity again. Quiet remains failed at 64.0%
p99 with zero hitches. The fresh 3000..4200 profile pair has identical native
source calls and zero interpreted/dirty/missed execution. Mean sampled source
CPU: off 53.63%, on 52.05%; the 2.375% setting-1-only publication stack is RAM,
PGXP-page and GPU-region copying. Receipts: `brief5-critical-profile-diff/`
(source, profile-analysis, leaf/inclusive diff, async-disassembly). Sampling is
attribution, not acceptance timing.

Framework `c30fc917` omits 2.38 MiB of GPU derived scratch from publication and
narrows PGXP publication pages (256 to 64 words). Both scratch buffers are
filled by their consumer before use; active checkpoints refuse capture. Private
partial-transfer/checkpoint, provenance, PGXP boundary/reuse and O0/O2 upload
history checks pass. Refreshing production and continuing ordered gates. No
true blocker; both branches remain unpushed.

The combined footprint candidate regressed quiet p99 to 100% despite identical
4,511-row guest identity, native execution, zero misses and zero >25 ms gaps.
Framework `dd49ca4d` restores the original 256-word PGXP publication pages first
and retains the private derived GPU scratch. Existing host-span receipts now
separate RPUB total publication, RRAM backing/metadata, RPRE precision and RGPU
GPU/GL capture. This directly attributes high-percentile windows; sampled means
alone do not establish their cause. Boundary/reuse tests pass. Refresh/gates
continue; loaded/cadence/title ctest/pushes remain pending. No true blocker.

Direct publication attribution (`brief5-worker-publication-spans/publication-attribution.json`)
measures RPUB mean 0.488 ms/job, p99 0.667/max 0.813; RAM p99 0.373,
precision 0.165, GPU 0.140 ms. Publication accounts for 1..3 points in the
worst windows; residual p99 is 63.3%. The hitch overlaps source disc reads
(8.2/13.0 ms). The paired profile's unprofiled 900..2990 prefix has off p99
58.1% (zero windows >=60) versus on 65.5% (12 >=60), supporting worker
contention rather than 36 points of source publication.

Framework `502072f9` runs optional guest draws only inside declared source
pacing waits. It admits against a measured execution estimate and cancels
private dispatch at the idle deadline. Source execution never waits for a job;
the worker also uses below-normal Windows scheduling, matching other optional
workers. Existing publication receipts also tag background RWRK execution.
Production compilation passes; refresh and ordered qualification continue. No
true blocker, push, PR, merge or release.

Idle-budget replay retains all 4,511 original-off identity rows. Both timing runs
have positive native source/worker execution and zero interpretation, dirty
instructions, dispatch/segment/worker misses. The loaded profile pair then uses
the same executable, route, 40 sections and defaults, with 23.19% / 23.21% host
CPU load. Native source calls are identical (67,568); worker publication is zero
at setting 0. Sampled source CPU is 59.39% off / 59.75% on. Publication is the
only setting-1-only stack (0.603% of samples). Receipts:
`brief5-loaded-profile-diff/{source,profile-analysis,leaf-diff,inclusive-diff,unprofiled-prefix-proof}.json`.
The unprofiled prefix ends before sampler attachment at frame 3450, so its busy
windows are separate from sampling overhead. At setting 0 it already contains
18 windows >=60% (seventh-highest 64.2%): enough to fail the full 60-second p99
bar even if every remaining window were below 60%. This is diagnostic evidence
of a baseline failure, not a claimed passing acceptance run.

**True blocker - scope decision:** The current production code's setting-0
loaded baseline already exceeds the <60% busy bar (unprofiled frames 900..3439:
p99 65.9%, 18 windows >=60%, no worker publication), while the paired setting-1
prefix is 67.1% and sampled source CPU differs by only 0.36 points. The latest
direction is to move exactly setting-1-only work and stop general hot-path
optimization; addressing the common loaded baseline requires work beyond that
restriction. Options: (1) authorize common emulation/context cost regression
work under load while retaining 40 sections and the existing acceptance bar;
(2) explicitly revise the load/bar. Quiet passes 60.000 Hz / zero hitches /
58.4% p99; loaded remains failed. Distinct-cadence qualification, setting-3
achieved FPS, final PC title ctest and both pushes remain pending. Both matching
branches are committed and unpushed; no PR, merge or release was made.

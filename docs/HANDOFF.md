## Phase 3 active continuation: native texture and region uploads (2026-10-29)

ONLY phase3, cadence and distinct-image gates. Fork main d2185504 already
merged in87c415a5; fetched again, unchanged. Phase4 separate topic nowbf2fb59e
records failed acceptance; do not work on it or phases3b/5. No phase3 promotion.
Current framework topic pushed WIPd30171b2. No mainline promotion.

Production packet texture coherence now uses captured native inputs without
GPU-to-native packing, independent of HD replacement mode. Per-packet native
tile versions avoid repeated copies/uploads; enhanced draw dirt no longer
dirties native sampling. Packet fixture now checks enhanced output without
synthetic texture-page uploads, plus version reuse:909checks pass.
Diagnostic native-route run first exposed117025 uploads/102.77GB in30s;
version reuse reduced to9531/4.53GB. GPU packet20.681->16.567ms, zero pack draws,
starved and misses, ~30distinct/s. Still fails cadence:entry35.958/return27.627ms.

Further changes: densely packed native PBO uploads shared with guest uploads;
removed CPU1555->RGBA conversion, full native RGBA staging texture and upload
vertex batches. GPU native R16UI seeds enhanced targets directly. Surface
handles/bounds now grouped inGlRasterSurface. Direct-wide valid copy coverage
excludes reveal margins; copies clip staging to destination bounds.
Hidden cropped1/4/9/18x, packet/history, bank+PGXP fixtures pass. Persistent and
portable upload stress including odd-width rows passed4630checks. Production still allocates full-VRAM HR: pool/selection/direct
wide output cutover remains the main unfinished work. No ten-minute gate.

Full framework build passed after repairing native snapshot fixture doubles.
Non-hardware ctest:301 enabled entries, three external/platform skips, two
additional disabled entries; one stale safety-expression source guard fixed
and rerun passed. Root dev32/32 and tools/smoke.py zero misses passed before
latest native-upload changes; latest dev32/32 also passed. Native seed race
60.019Hz,entry37.227/return34.424ms,29.998distinct/s,GPU16.628ms,zero starved/misses.
Present-slot server-fence GPU timer added,914 fixture checks pass; dev build
for its race remeasurement is now running. All runs
dummy audio, hidden only; all prior owned processes exited. Receipts and exact
rows in docs/VERIFIED.md and analysis/perf-20261028. Continue; no owner-only
blocker and no final acceptance claim.

## Phase 3 continuation: direct regions and hitch attribution (2026-10-29)

Still phase 3 ONLY. Topic `feat/frame-arch-3-surfaces` has pushed WIP
`99d4f612`; no promotion, root pin still accepted phase 2. Fork main remains
`d2185504`. Do not do 3b, 4 or 5. All runs use dummy audio and hidden windows.

Direct-wide/cropped fixtures now pass through18x: ordinary/backdrop flat and
textured geometry, lines, overlays, uploads, fills, masks, cross-target copies,
display extraction, captures and transaction coordinates. Texture output is
byte exact; ordinary fixed-function Gouraud permits one RGB code (362pixels
at18x); mask/alpha and all page positions are exact. Receipt
`phase3-direct-wide-18x/rth-nq_cv7nr`. Dev build and R4 32/32 passed.
Subsequent uncommitted region allocator consolidation and multi-tile copy gather
pass `phase3-region-allocation-18x/rth-9_kedxl7`; production STILL allocates
full-VRAM HR. Surface pool/selection/direct-wide cutover remains the main work.

Packet vector growth no longer zeroes bytes that the producer immediately
overwrites. Opt-in allocation/attachment cleanup timings identify their cost
in the worst cadence interval; frame mailbox tests pass. Latest short receipt
`phase3-allocation-timing18`:59.991Hz,entry35.670ms,return23.913ms,zero starved,
GPU16.513ms,953changed32x18samples/30s=31.766distinct/s,zero missing receipts.
Worst allocation .079ms, cleanup .036ms, native submit .217ms/sync .276ms,
guest20.176ms and previous pacer14.934ms. This is NOT a pacing fix or gate.

Diagnostic sampler on/off experiment did not explain stalls: return26.918/
25.839ms. Another non-owned runtime PID41464 was discovered during the pair;
left untouched, subsequently exited. Those runs are not isolated GPU proof.
WPR was denied OS profile-policy permission (0xc5585011), no trace started.
Existing host span/frame rings plus absolute-frame sampler windows are being
used instead. New worst_frame marker and host_profile --frame-lo/--frame-hi
connect the worst interval to those existing probes. Continue the production
surface migration and ten-minute gate; no owner-only blocker, no final claim.

## Phase 3 active scope and latest evidence (2026-10-29)

Supervisor scope: ONLY phase 3 and guest-stall/distinct-image gates. Phases 3b
and 5 are already merged; phase 4 belongs to another agent. Do not redo them.
Framework topic feat/frame-arch-3-surfaces merged fork r4/main d2185504 in
87c415a5. Root pin remains accepted phase 2 until phase 3 passes.

Current work removes production PGXP depth replay (unbiased depth in the
colour pass), preserves the opt-in epsilon audit, and fixes masked fragments
writing invisible depth. Hidden analytical/raster-origin/mask and cropped
1x/4x/9x fixtures pass. Race visual/decal acceptance remains open.
Present trace now uses eight fenced PBOs and a 32x18 image receipt without
per-swap CPU readback waits. Changed hashes prove distinct images, while
unchanged hashes cannot prove equality. The 30 s receipt counted 1800 changed
samples with zero missing receipts (60/s lower bound).

Native draw transport was regressed to per-command mutex/SetEvent by the
imported checkpoint code. Restored SPSC parking with retained events, retaining
checkpoint sequence ordering and borrowed upload lifetimes. Native pixel,
cache and two-checkpoint tests pass 20 repetitions. Worst-interval submission
cost fell from .533 to .245 ms, but overall worst return increased from21.198
to22.330ms: NOT a demonstrated pacing fix. Zero misses/starved in both runs.

Release build build-phase3-release completed (PSX_DEBUG_TOOLS=OFF). Hidden
30-second slot10 benchmark:59.997Hz,entry max33.358ms,callback return18.519ms,
zero return intervals over20ms and zero audio underruns. Worst entry includes
14.931ms previous pacing plus17.862ms guest work. GetThreadTimes is quantized;
its per-interval CPU field must not be treated as accurate off-CPU attribution.
Release harness analysis/perf-20261028/release.py uses hidden owner-PID resize
and graceful WM_CLOSE, always dummy audio. Raw held Cross does not establish
analog driving; this is NOT the ten-minute moving-race gate.

Full-VRAM HR allocation, canonical/wide duplication and scaled RTT surface
cutover are STILL OPEN. No phase acceptance or merge into fork r4/main.
R4 32/32 and cropped target parity pass after these changes. No live processes.
Continue production surface migration and representative long-run gate without
ending at this checkpoint. All runs must use SDL_AUDIODRIVER=dummy.

## Phase3 latest continuation (2026-10-29)

Framework phase3 topic98f3a4ac and subsequent source-origin fixture pushed, not merged: native-input-only batch
flushes removed, cropped target projection/upload/fill/copy parity groundwork.
Subsequent cross-target copy source and translucent-wide-overlay projection
fixes pass hidden fixture phase3-cross-target-overlay/rth-68qop30w:18 full/crop
colour/depth/wide comparisons and72 mask-copy combinations at1x/4x/9x.
Dev build and R4 32/32 passed. Same slot10 forced18x all-on receipt
phase3-input-batch-slot10-18:60.044Hz,max37.413ms,zero starved,60swaps/s,
GPU packet15.693ms (8.994present),7.3batches vs69.7,render169.79ms/s.
Swaps are NOT distinct images. No phase3 promotion or final target claim.
No live game/build/test. Continue phase3 production surface pool/direct-wide,
scaled RTT and one-pass depth, then3b/4/5; no owner-only blocker.
Do not simply enable HIW: it disables transactions/extra draws/live scaling
and still duplicates native plus scaled column-window draws. Cropped-target
helpers are tested but production still full VRAM. CPU native-input helper
updates HD keys/coherence only; it does not upload immutable tiles to raw GPU
texture. Preserve actual texture mutation flushes. Cross-target copy helper
currently assumes source/destination equal scale. Phase4 remains100% match.

## Phase3 continuation (2026-10-29)

Current framework branch `feat/frame-arch-3-surfaces`, based on accepted phase2
integration a8a62e54. Topic commit98ff69cb is pushed (not merged). Work adds scissored depth clears, bounded persistent vertex
streams (GL3.3 fixed-storage variant), and nonblocking packet GPU timestamps.
Old depth-clear geometry/mode removed; colour/depth replay and full VRAM remain.
Dev build/R4 32 tests, hidden stream4110checks, PGXP1x/4x/9x, packet74checks pass.
Framework full build passed; 295 non-hardware unit tests pass after regenerating
the TCP command index for the imported PGXP audit commands.
Hidden smoke frame9754 zero misses, PID7452 stopped. New slot10 benchmark
required by execution identity change; old slot9 preserved. Slot10 18x all-on:
60.044Hz,max41.345ms,zero starved,51.2swaps/s(not distinct),GPU packet18.948ms
including12.509ms presentation. PID15980 stopped. No live game.
No phase3 acceptance or promotion. Continue direct-wide/on-demand RTT surfaces,
one-pass depth, then3b/4/5. No owner-only blocker. Do not finalize at checkpoint.

## Current rollout boundary: phase 3 (2026-10-29)

Phase2 accepted and promoted: framework topic27d6d167, merged with independently
landed fork PGXP audit0e97c44d at a8a62e54, pushed to fork r4/main. R4 pin update
includes required regeneration (no generated changes). Normal18x active-effects:
59.983Hz,max43.919ms,zero starved,55.07swaps/s; owner delayed50ms:60.093Hz,
max41.906ms,zero starved,16.37swaps/s. Distinct images not yet measured; these
are swap completions. Full framework295unit tests plus R4 32 pass. Imported
PGXP CPU5tests and hidden raster-origin1x/4x/9x pass. Hidden smoke, live/paused
queries, async resolution/native parity and capture-mark completion pass.
All started game PIDs stopped. No owner-only blocker. Continue phases3,3b,4,5.
Legacy transaction/XR APIs and old GL fixture assumptions still need migration
or deletion with their replacements, never reintroduce context borrowing.
Old all-on receipts did not execute Car Effects; new harness requires draws.
The current deadline check fixed starvation without lowering guest fidelity.

## Latest continuation (2026-10-29)

Phase-2 slowed-consumer gate now passes: `packet-deadline-stalled18`, 60.093
VBlanks/s, max41.906ms, zero starved,16.37swap completions/s, zero misses and
context/queue waits. Optional guest redraw admission checks current remaining
pacer deadline against measured cost; tests prove late refusal and recovery.
The earlier Car Effects-enabled receipts did NOT execute effects: session
activation erased the constructor-registered HUD callback. Both R4 plugins now
re-register their shared layer on activation. Active-effects normal run before
deadline fix:59.359Hz,max53.539ms,7630starved,44.76swaps/s.
New runtime regression `tools/test_car_effects_runtime.py`; perf harness now
asserts effects draw count increases. The 32 project and six focused framework
tests pass. High-resolution screenshot commands now queue immutable display
inputs to the owner, and wide captures allocate the actual owned dimensions.
Current hidden smoke/live-scale/effects gate is still in progress. No phase-2
merge or pin yet; continue without stopping, no owner-only blocker.

# Handoff

## Latest: native authority promoted; continue phase 2

**Latest continuation after `1d14aef6` (uncommitted):** native command queue
uses a bounded 4096-slot SPSC handoff with condition variables only for parking.
Native oracle/pixel tests passed 20 repetitions; a new full-queue wrap/order
fixture also passes. A deque batch experiment showed no benefit and was reverted.
Packet pending state is now exposed: old extra-draw admission incorrectly tested
a two-frame FIFO threshold, impossible for a one-pending-slot mailbox. Pending
admission is tested, but the 50 ms stalled-owner gate still fails:
`packet-pending-admission-stalled18` 58.645 VBlanks/s, max 46.863 ms, 10648
starved; all queue/context waits and dispatch misses zero. Do not accept phase 2.

Root 32/32 passed. Two smoke attempts exposed legacy `render_wide_display`
readbacks. Normal missing-wide presentation now decides its canonical fallback
on the GL owner. The actual screenshot command also needed migration: it now
copies guest display parameters and runs the readback/PNG on the owner, with
allocation sized from the current owned surface. Third smoke passed through
frame ~9862, zero dispatch/segment misses; 12 live plus 12 paused diagnostics
pass with zero acquisitions/releases. Evidence `phase2-cutover-capture-runtime`,
owned PID 9212 stopped. Earlier failed receipts are preserved in
`phase2-cutover-runtime` (PID 34720) and `phase2-cutover-wide-runtime` (43916).

Live resolution and startup ownership now pass a hidden runtime gate:
`phase2-startup-live-scale-runtime`, owned PID 31012 stopped. Full smoke plus
live/paused diagnostics has zero misses/acquisitions; paused 720p -> 1440p ->
8K allocations complete asynchronously and preserve byte-identical native PNGs.
Owner startup precedes all guest execution; PSX_RENDER_THREAD opt-out removed.
The full-PGXP bank fixture also exercises 1x -> 3x owner allocations, all four
cases pass (495/515 checks), exact on/off endpoint digests:
`phase2-owner-scale-fg-bank/fg-v4akfmyc`.

No framework merge, no pin, no phase 3 work. Continue all phases; no owner-only
blocker. Remaining legacy GL fixtures/host APIs, timing gate and performance
instrumentation still need work.

**Production cutover WIP committed/pushed as `1d14aef6`:** the blocking ring is
replaced by FrameOwner complete packets with owned native seeds, per-draw inputs
and producer draw state. Controls and diagnostics are durable owner requests.
Native worker now registers Games MMCSS. No phase-2 acceptance or merge yet.

- `packet-cutover-timers18.*`: 60.124 VBlanks/s, max 41.642 ms, zero starved,
  51.23 swap completions/s (not distinct). Seed 0.171 ms/frame including
  disposal 0.076, seal 0.033. Owned PID 25708 stopped.
- `packet-cutover-stalled18.*`: explicit 50 ms owner delay, 57.653 VBlanks/s,
  max 50.350 ms, 11869 starved, 14.14 swaps/s. Zero context/queue waits,
  zero misses; **fails phase-2 slowed-consumer gate**. Main CPU ~914 ms/s.
  Owned PID 34680 stopped. Do not claim GPU independence implies headroom.
- New owner/mailbox unit tests pass, including fresh-frame-before-tick and
  test-only paused control acknowledgement. Hidden real-GL complete-packet
  fixture passes 71 checks (`phase2-cutover-packet-gl/rth-69hl3f1d`).
- Migrated FG bank fixture binds native authority, acknowledges each real
  endpoint (test only), and inspects after owner shutdown instead of context
  borrowing. All four full-PGXP cases pass, exact on/off real-image digests:
  `phase2-cutover-fg-bank-ack/fg-h9bvoh1y` (489/509 checks each).
- Present slot admission is now nonblocking for both untimed and timed
  outputs. Full slots refuse without surrendering surface/fence ownership;
  GL disposes the unsubmitted fence. Old admission-wait configuration removed.
  Updated present_thread_test passes. New JSON refused counter.
- CPU-dual/HD-mode/dynamic-resolution controls now queue on the owner.
  Current dev build is being refreshed after these changes.

Remaining: stalled-consumer timing, legacy GL fixture migration (especially
HD), reachable GL_RT_SYNC APIs/live allocation/XR, startup ownership, full
acceptance. Old synchronous API now aborts rather than borrowing GL; do not
ship reachable host-feature failures. Phase 3+ not started. Continue work;
there is no owner-only decision blocker.

Phase-2 topic is `feat/frame-arch-2-mailbox`, based on `4f893c1a`.
WIPs through `1d14aef6` are committed on the topic; phase 2 is not merged or pinned.

Latest continuation adds a real hidden-GL complete-packet gate: the new owner
replays actual RTH commands with full draw-state seeds and per-draw native
texture inputs, replacing 62 pending frames while stalled and retaining inputs
after native-worker destruction. 71 checks pass, with GL context transfers only
at startup/shutdown. The owner also cancels old-timeline tick deadlines before
reset work; 20 repeated concurrent runs pass. Optional private record batches
append atomically, and cancellation preserves the last complete pending frame.
Native facade binding exposes a CPU upload-flush boundary. Dev build, R4 32/32,
hidden smoke (frame 8496, zero dispatch/segment misses), and 12 live plus 12 paused
diagnostic requests pass with zero context acquisitions/releases. Owned PID
24552 stopped. Receipts: `phase2-packet-replay-gl-final/rth-grkmih5g` and
`phase2-packet-owner-runtime` under `analysis/perf-20261028/`.

The production GL transport is still the old ring. The new fixture validates
the consumer boundary, not production integration or phase-2 performance.
Continue the actual packet cutover; no owner-only blocker is identified.
The audio callback now exclusively owns the resampler, fed by an SPSC PCM
transport; main/callback register Games/Pro Audio MMCSS. The old push-audio
diagnostic switch is removed. Load/rewind requests discard old-timeline PCM
on the callback without discarding subsequently produced samples. Exact
resampler parity, concurrent wrap/fill and timeline-reset tests pass.

The bounded three-slot frame packet transport is implemented and tested but
**not yet wired into the GL command path**. Tests cover a stalled consumer,
owned 1 MiB seeds, command order/alignment, whole-packet overflow rejection,
timeline invalidation, and native pixel equality after skipping 62 pending
packets. Presentation seeds can observe native VRAM/cache without consuming
the dirty rows needed by rewind. Dev build and R4 32/32 pass; focused native,
mailbox and audio tests pass. No phase-2 race acceptance claim or merge yet.

Further phase-2 work adds `frame_owner.h/.cpp`: a dedicated consumer with
startup/shutdown-only context transfer, bounded newest-wins consumption,
load epochs, consumer-thread identity, Games MMCSS and timed idle callbacks.
Its blocked-consumer and idle-tick tests pass. GL recording now maintains a
producer-owned persistent draw-state mirror and restores it across ordinary
and async extra passes (including discard). Six hidden GL modes retain pixel
parity. Latest dev build/32 tests and hidden smoke/12 live+paused queries pass;
owned PID 29628 was stopped. Receipts are `phase2-state-pass-gl/rth-rk999nru`
and `phase2-state-runtime` under `analysis/perf-20261028/`. The new owner is
still not connected to the GL backend; do not claim queue backpressure fixed.

2026-10-29 continuation: native HD contexts and immutable Duck/Beetle
provenance snapshots are implemented. Native context configure/reload/tracking/
diag tests make zero GL calls; two consumers can apply a held Duck snapshot
independently after source destruction. Pending copy state survives capture.
Packets can own one bounded CPU-only attachment; held/replaced/invalidation
lifetime tests pass. These APIs are not yet connected to native raster jobs or
production GL packets. Five focused tests pass, dev build and R4 32/32 pass.
Hidden HD fixture passes 231 full-surface and 217 tiled checks with PGXP depth,
colour perspective and seams on (`phase2-hd-context-gl/final-receipt.json`).
Seven old fixture failures reproduced with the previous HD implementation;
updated fixture explicitly restores caller memory and checks private renderer
reads instead of expecting guest VRAM publication/context holds. Hidden smoke
and live/paused diagnostics pass, zero misses/acquisitions; owned PID 27708
stopped (`phase2-hd-context-runtime`). No phase-2 performance claim.

Next: replace the incremental GL ring with independently seeded packets,
including full persistent render state, durable controls and exact native HD
texture inputs. Capture the seed before the first native GP0 of its packet;
capturing lazily at the first presentation call would already include that
draw and double-apply semi-transparency. Do not drop the existing incremental
records or silently discard HD upload/copy provenance. Preserve retained
enhanced surfaces when frames are consecutive, and account for missed writes
when a packet is replaced. Then run the slowed-GPU phase-2 acceptance gate
and continue phases 3, 3b, 4, 5. No owner-only blocker is identified.

2026-10-29 next continuation: native raster observers now track original GP0
draws, masked/fragmented uploads and copies, with write bounds independent of
rewind dirty rows. Main binds the native HD tracker at startup; configuration
prepares both trackers before commit. Native sampling marks Duck uploads used
even when presentation is dropped, preventing erroneous adjacent-upload
coalescing. Dump observations belong to the native tracker. Immutable packet
tracking shares immutable source pixels and excludes authoring history.
Direct packet seed reservation avoids an intermediate full VRAM copy.
The presentation owner now has a bounded, durable async request queue, tested
with a stalled consumer, replacement, owned arguments and timeline reset.
It remains a component: GL still uses the old ring. Exact native texture inputs
at each draw (including write-then-sample in one frame) remain a cutover
dependency; a frame-start snapshot alone is insufficient.
Dev build, R4 32/32, focused tests and the registered hidden HD GL fixture pass.
Hidden smoke reaches race with zero misses; live/paused diagnostics retain zero
context acquisitions/releases. Owned PID 46044 was stopped. Receipts:
`analysis/perf-20261028/phase2-native-provenance-runtime/`. No performance gate,
phase-2 merge or pin change yet.

Further WIP: `NativeRasterInputs` now owns per-original-GP0 sampled pages and
CLUTs in shared/versioned 64x64 native tiles, captured before the draw. HD
provenance is retained at the same point, shared until its generation changes.
The collector is bounded (64 MiB logical); overflow drops the collection, not
native guest work. Tests cover write-then-sample, wrapping, restore/lifetime,
overflow recovery and exact inputs after 62 mailbox replacements. Production
does not enable/seal these collections yet. Original GP0 IDs now travel through
the producer facade into RTH_NATIVE_SOURCE records; they identify per-packet
inputs, not cross-tick interpolation identities. Host-only work uses zero.
Texture-bank selection resolves immutable CPU assets on the producer and
queues their GL creation/selection without taking the context. Bank state is
included in legacy generated-list capture/replay/restore. Six hidden GL parity
modes pass (`phase2-native-input-source-gl/rth-pq5xrw76`); full-PGXP bank
frame-generation tests pass both present paths with identical real digests
(`phase2-bank-framegen/receipt.json`). The generation fixture now awaits queued
controls before asserting their applied state; its former assertion failed
identically without banks (`without-bank.json`). Native/parser focused 12 tests
and GP0 history test pass. Production still uses the blocking ring: next is the
actual complete-packet/owner integration, not phase promotion.
Latest dev build and R4 32/32 pass. Hidden smoke/live+paused queries pass with
zero misses and zero context acquisitions; owned PID 39400 stopped. Receipts
are `analysis/perf-20261028/phase2-native-input-runtime/`. The bank replay test
is now registered as `gl_frame_gen_bank_test` and passes. Owner controls now
run before their named frame (or a replacing newer frame), diagnostics after;
an idle control cannot be stranded waiting for another guest frame.

Framework phase-1 topic through `464bc104` is merged/pushed to fork r4/main
at `4f893c1a`. Original GP0/native pixel and snapshot fixtures pass. The race
records zero GL context acquisitions, but render-queue backpressure remains:
53.075 VBlanks/s,max67.360ms,79173starved,53.16swap completions/s at18x/all-on.
Native display history and pure-render diagnostics have no GL drain/context
move. Full framework build/unit gate, R4 32 tests, six hidden GL modes and
hidden smoke frame8767 pass; live/paused renderer queries pass. All owned
processes are stopped. Phase-1 gate is specifically native authority/race
context ownership; non-race legacy ownership APIs migrate with phase2 packet
controls and transactions disappear in phase4. Exact native HD texture inputs
must travel with independent packets. See FRAME_ARCHITECTURE sequencing note.

Continue phases2,3,3b,4,5 without ending at a checkpoint. Phase4 match bar is
100%. No R4 PRs, packaging, visible windows or display changes.

The following records are historical and superseded where they conflict.

## Current work: native authority integrated, phase 1 still open

Latest framework WIP is `1f5f6e1a`, pushed on the phase-1 topic, unmerged.
It queues renderer controls and immutable CPU/depth24/blank/hold images,
publishes diagnostic snapshots, removes handoff VRAM publication, and carries
oversized records through bounded transport fragments instead of GL migration.
All six hidden GL modes, transport tests, build/32 R4 tests, and hidden smoke
pass. Latest same-save all-on 18x run: 53.353 VBlanks/s, 64.265 ms maximum,
78556 starved samples, 53.33 swap completions/s, zero context acquisitions.
Earlier repeats varied substantially (9.643 and 43.031 VBlanks/s); do not claim
causal performance acceptance. Details and receipts are in VERIFIED.

Work after that commit: thread-safe completed-thumbnail publication removes
present-image-ring GL drains. Its concurrent Windows writer/reader test passes.
An asynchronous render-owner diagnostic callback is being integrated into the
TCP server for pure-renderer handlers (coherency ring, present ring, post-AA).
It uses IO_EXEC ownership and thread-local response append ownership so the
emulation thread continues while the renderer produces a response. Dev build,
32 R4 tests, hidden smoke (zero misses), six GL fixture modes and live/paused
TCP queries pass. Additional pure-renderer queries (shader, bloom, batch sites,
interpolation) are covered too. These changes remain uncommitted. The full
framework unit tree in `psxrecomp/build-prsync` is currently rebuilding; it is
Release/debug-tools-off, but is not the final R4 Release performance build.
Do not merge/pin until remaining phase-1 ownership paths and gates are resolved.

Continue the full rollout through 2, 3, 3b, 4 and 5 without stopping at a
checkpoint. Phase 4 requires **100%** matching of primitives present in both
packets (owner commit `26b54a3`). No phase has passed acceptance or merged.

On `feat/frame-arch-1-native-authority`, original GP0 inputs now reach the
native worker before widescreen transforms and RGB555 conversion. GPUREAD,
native scanout, savestates and rewind VRAM read native CPU memory. Snapshot
wire preserves texture/palette cache state and partial uploads. Reference
coverage: 10000 mixed draws, 10000 coordinate cases, 2000 interlaced draws,
2000 fill/copies; all exact. Fixed interlaced fills and odd GPUREAD high halves;
write-invalidated row caching removes per-halfword worker round trips.

Dev build, R4 32/32, ten parser/native/scanout framework fixtures, actual-parser
debug/production parity and hidden boot-to-race smoke pass (zero misses).
Latest integrated measurement `native-authority-integrated-18.json` is still
39.716 VBlanks/s, max 77.948 ms, 224724 starved samples. Remaining phase-1 work:
renderer controls/diagnostics still migrate GL context; old transaction and
backend publication paths remain. Mailbox must use independent native inputs,
not drop incremental GL records that future frames depend on. No active game
process remains from that measurement. R4 gitlink remains `ac65fcf5` deliberately.

The following section records the earlier component-only checkpoint; its
"not integrated" statements are superseded by the status above.

## Previous checkpoint: frame architecture, 2026-10-28 (phase 1 incomplete)

Follow `docs/FRAME_ARCHITECTURE.md`, including owner amendment `b832af3`:
phase 3b audits PGXP; phase 4 is generic packet interpolation, not private
guest redraw workers. No phase has passed acceptance or been merged yet.

Framework checkout/topic `feat/frame-arch-1-native-authority` is at
`643af69d`, pushed to `fork`, based on fork `r4/main` `ac65fcf5`. WIP
`b56f4914` adds independent SW raster contexts and an ordered CPU-only native
queue. Partial A0 fragments preserve masked destinations, wrapping, received
prefixes and snapshot/resume between fragments. Tests and build/smoke results
are in VERIFIED. Runtime does not yet call the native queue. `643af69d` additionally adds a
native-only RGB888 polygon path with integer attribute planes and dithering;
hand-known gradient/dither fixtures pass, but full oracle parity and original
GP0 capture/routing remain outstanding.

Next integration boundary: capture native command inputs before GPU parser
widescreen/HUD transforms and RGB555 truncation. A facade-level duplication
would inherit those presentation changes. The existing software raster lacks
dither state and uses reduced-precision Gouraud interpolation; the historical
accuracy ledger already identifies this. A pinned local Beetle reference is
at `analysis/perf-20261028/beetle-reference`, commit
`53246dc2d00f3c00cc56baa1061902da7a6651e4`; no reference code was copied into
the framework. Hardware pixel parity remains unverified.

The earlier `fix/queued-vram-snapshot` commits `a1af76d0` and `1efd81f9`
remain preserved, unmerged. Their render-owner completion should be replaced
by native completion in the new design. R4's gitlink remains `ac65fcf5`;
do not mistake the modified local submodule pointer for a promoted pin.
No visible windows, display-mode changes, packaging, installation or PRs.

## Current work (2026-10-28, owner PC real-time performance)

Final gates at framework `ac65fcf5`: regen unchanged, build, 32/32 ctest,
hidden boot-to-race smoke with zero dispatch/segment misses. Fresh defaults
resolve to 9x internal at an actual 7680x4320 drawable. Clean 25-second
recheck: 59.977 main callbacks/s, zero audio starvation, zero whole-period
pacing debt entries and 30.00 presents/s with zero presentation hitches.
The separate image-traced run confirms 30.00 distinct images/s; readback
perturbed its pacing, documented in VERIFIED. Default and optional-worker
composed screenshots were inspected. Receipts are under `final-gates/` and
`acceptance-defaults*` in the evidence directory below.

Performance defaults: retain PGXP including CPU propagation, Car Effects,
borderless fullscreen, fixed resolution and the existing 4K/9x Match display
cap. Rewind and the Race frame rate enhancement become opt-in. The owner's
8K output now uses original 30 FPS race images at full emulation speed;
60 FPS enhancement performance is **not restored**. Same-save hidden GL
25-second comparison: 18x old defaults 50.898 main callbacks/s, 171184
starved audio samples, 25.48 distinct images/s; new 9x/original-frame
defaults 60.049 callbacks/s, zero starved samples, zero pacing entries a
whole period late, 30.04 distinct images/s. Full table and measurement
semantics are in `docs/VERIFIED.md`. All measured runs had zero misses.

Rewind synchronously reads back VRAM during machine snapshots. Extra race
draws overload this 8K configuration even after enabling private workers
by disabling Car Effects. CPU mode, seams, bloom and presenter slot count
did not solve that. Controller enumeration is event-driven; the closed
overlay is not doing frame rendering. The optional worker scheduler now
uses exact indexed rational boundaries after stalls/lead skips, preventing
spurious near-one redraws; regressions use the actual framework planner.

Ghidra setup is complete at `C:/Users/Shokunin/dev/ghidra-mcp`: official
Ghidra 12.1.4, Temurin JDK 21, Python 3.12 venv/pyghidra-mcp, analyzed
OpenBIOS at BFC00000 and SLUS_007.97 text at 80010000. `ghidra` is registered
in the owner's Codex config. MCP stdio verification lists 1460 game functions;
setup and receipt are local `README.md` and `stdio-verification.json` there.

Local performance harness/evidence: `analysis/perf-20261028/`. Slot 9 is a
fresh GP grid save. Debug performance runs must disable DISPLAY_RING,
PRESENT_IMAGE_RING, GL_PERF and ASYNC_GPU timer/ops unless profiling them;
the debug defaults are not representative of the release renderer. No
visible game windows were opened and the display mode was never changed.

## Current work (2026-10-27, PGXP seam strips)

The owner's "pgxp was buggy" on the 0.2.1 candidate (8K/18x) was seam
snapping (`pgxp_seam = "fine"` strips), not geometry correction, depth or
colour: a strip ran along a whole edge at its largest gap with the texture
extrapolated past the polygon (black), only 3% behind its plane, against a
depth buffer the frame's mid-frame clears had emptied -- black / grey bars
along kerbs and lane marks, wedges on wall tops. Fixed in fork `r4/main`
`9d93976c` (PR [psxrecomp #5](https://github.com/tetrisgm/psxrecomp/pull/5),
ENHANCEMENTS.md G1.15 "Strip extent and depth"); R4 pins it. Evidence and
the hidden-window capture harness (tick-exact A/B from the attract-demo
save, `drive.py`, `imgtools.py`) are in
`C:/Users/Shokunin/dev/r4-pgxp/analysis/pgxp`. Not verified: a real
8K fullscreen playtest (hidden window only, owner rule), the player's chase
camera over a full lap, and 30 FPS vs 60 FPS flicker of strips in
in-between frames (the prepared batch now matches the live path: 0
mismatches under `PSX_SEAM_PREP_VERIFY=1`).

## Earlier work (2026-10-27, 8K dynamic resolution)

Dynamic resolution at 8K/60 stayed at level 5 (1200 lines) because its
cost was max(CPU, GPU) with GPU one `GL_TIME_ELAPSED` span per frame: that
span counts GPU idle while waiting for the render thread, so it read
15.5-16 ms of the 16.7 ms interval (load 0.94-0.99, "over budget at the
floor") while per-record GPU work was ~0.2 ms. Framework `e4efba40`
(fork PR [psxrecomp #4](https://github.com/tetrisgm/psxrecomp/pull/4), `feat/dynres-gpu-busy`) makes the cost the CPU time
(`PSX_DYNRES_GPU_COST=1` restores), adds the queued extra frames'
render-thread time (enter/replay/finish/compose, which carries a
GPU-bound level's fence waits; `PSX_DYNRES_ASYNC_COST=0`) and counts a
window short of the presenter's target rate by more than 1.5 outputs as
over budget. It also carries the per-stage/per-record GPU timer diagnostic
(`PSX_GL_ASYNC_GPU_TIMER=1`, `PSX_GL_ASYNC_GPU_OPS=1`, stderr at exit).
R4's frame-rate workers no longer let the output grid ratchet ahead of
now: a tick arriving while the grid is more than `R4_OUTPUT_LEAD_MAX` (2)
windows ahead reserves nothing (0 keeps the old behaviour); before, every
catch-up tick after a stall added a window of presenter latency until all
eight slots were full and the render thread waited ~12 ms per output
(`timed_waited` 666-683 per 15 s, "compose" 8-10 ms).

Bound at 8K/60 with the 60 FPS mod, forced levels, 7680x4320 drawable,
race save, 15 s each (`analysis/k8-f*`, GPU timer diagnostic on, which may
inflate GPU utilisation; `nvidia-smi` 200 ms samples): 18x: GPU 90 %,
151 W, replay span 15.3 ms/output, render thread saturated (finish 23.5 +
compose 22.1 ms per extra output), 21 presents/s. 13x: GPU 79 %, 138 W,
replay 9.8 ms, render thread 56 % busy (12.7 ms per extra output), 58
presents/s with 12 refusals. 9x: GPU 57 %, 106 W, 6.5 ms, 32 % busy, 60/s.
5x: GPU 42 %, 80 W, 4.1 ms, 32 % busy, 60/s. The guest ran 60 Hz at every
level. The GPU is the bound from about 13x up (fill at 8K plus the 8K
compose); below that the render thread's CPU is, and at 5x-9x neither is
near its limit. With the fix (one 45 s on-screen run taken before the
owner's screen rule, `analysis/k8g-new45w`): 5 -> 13 in 23 s after a 20 s
settle, 2657 distinct DXGI frames in 45 s (59.0/s, worst gap 188 ms at
the window placement, otherwise 67 ms), presenter 888 distinct/15 s, load
0.55-0.58 at 13x, GPU about 40 %, 106 W. Before (`k8d-old`): level 5 for
the whole run. The climb is one level per 3.25 s (`up_after_s` 3 +
cooldown); where it settles between 13 and 16 on a quiet 8K panel, and
whether it oscillates there over minutes, needs an on-screen run when the
owner is away from the PC. Also seen: the worker grid reserves two
in-between outputs per tick in some runs and one in others (`extras=`
in the shutdown line), which doubles replay work; not changed here.

## Current work (2026-10-26)

Render-thread CPU per output (2026-10-27, clone `C:/Users/Shokunin/dev/r4-glctx`,
fork `r4/main` now `0bcc5a49`, R4 pins it). First the GL fixture harness: nine
fixtures no longer compiled on `r4/main` (duplicate telemetry/menu/sampler
stubs after the merges; `gl_async_expire_test` registered with a
`--compare-env` the runner never had); repaired on `fix/gl-fixture-harness`.
The mask-mode scale check then failed for real: with accurate blending (the
default) a modulated texel came out 1/255 brighter (white 248 -> 249) because
the shader multiplied by colour/255*2; it now computes (texel8*colour8)>>7.
Then two measured levers from the glctx findings: HD texture-pack tracking
(every textured draw queried the pack with three vectors, a CLUT hash and a
lease allocation; every primitive's invalidation note split its rect into a
vector) is allocation-free on the miss path and index-driven, and the
presenter waits up to `PSX_PRESENT_ADMIT_WAIT_MS` (25 ms, 0 = old refusal) for
a slot instead of dropping a finished timed output. Hidden 1440p/120 probes:
replay 2.7-3.5 -> 2.0 ms per output, HD_NOTE 944 -> 71 ms and TEX_TRI 1898 ->
1334 ms per 15 s, refusals 62-200 -> 3, presents 1642-1741 -> 1799/1800. The
physical 1440p/120 DXGI count did not move on this quieter host (base 111.9,
115.2; new 111.1, 110.3 distinct/s, presenter at 120 presents/s in all four):
the remaining loss is duplicate presents in one-second dips, not render-thread
time. Still on the render thread per output: compose/postfx (postfx_present
~10 % of samples, temporal mask/depth capture 2.5 %, window copy), PGXP seam
snapping (~6 %, already a hashed index), per-record exec dispatch (~3 %),
software raster of the live frame under HD authority (~3 %). Dynres: `gpu_ms`
is one GL_TIME_ELAPSED query around the whole frame, so it is the GPU-side
span including waits for the render thread, not busy time; at 8K/60 it reads
15.4-15.8 ms and holds dynres at level 5 (1600x1200) while per-record GPU work
is small (glctx: ~0.15 ms per output), so the 8K internal resolution is
CPU-span-limited. A busy-time measure would need per-batch queries summed
(as `PSX_GL_ASYNC_GPU_OPS=1` does); not built.

Second GL replay context for the extra frames: measured, not built
(2026-10-27, clone `C:/Users/Shokunin/dev/r4-glctx`, fork PR
[psxrecomp #3](https://github.com/tetrisgm/psxrecomp/pull/3), branch
`feat/gl-replay-context` on `ab9d181d`, unmerged, R4 pin unchanged). The
panel lists 1440p at 120 Hz at most and no 144/165 mode. Three physical
1440p/120 DXGI runs from a fresh race state gave 86.1, 90.6 and 67.3 distinct
frames/s (worst gaps 76/30/58 ms); two lost outputs to one 134-139 ms
present-thread swap stall that filled the eight presenter slots, so the
shared host's noise exceeds any gain a context change could show. The new
`PSX_GL_ASYNC_GPU_TIMER=1` diagnostic (per-stage `GL_TIME_ELAPSED`) shows the
replay's GPU span at 5.1-6.0 ms per output against 3.4-4.3 ms render-thread
wall, while `PSX_GL_ASYNC_GPU_OPS=1` (per-record queries) sums all replayed
records to about 0.15 ms per output, unchanged at a forced 3x scale: the GPU
idles waiting for the render thread (the dynres `gpu_ms` of 16-17 ms is the
same span measure, not GPU busy). The render thread's own CPU work is the
bound: per output about 5.4 ms (textured-triangle setup 2.5, compose 1.5,
HD texture-pack notes 0.85; host samples: postfx 13 %, PGXP seam snap 5 %,
HD pack tracking ~12 %, `operator new` 4.5 %, software raster ~3 %). A
replay-shaped two-context microbench (`analysis/ctxload/ctxload.c`, RTX 4080
SUPER, driver 610.88) is never faster than one context (1.56 vs 2.0 ms per
output; 25-40 % slower across three workloads), and Codex's shared-context
fixture, now registered on the branch, measures 42 vs 56 ms. Hidden-window
120 Hz A/B on a quiet host: default 1786 presents/15 s at 74 % render busy;
`PSX_PGXP_SEAM=0` cuts replay to 2.34 ms and busy to 48 % but then 207
timed presenter refusals appear (`PSX_BLOOM=0`: 579), so once the render
thread has slack the presenter's slot admission drops outputs instead. Next
lever is the render thread's CPU per output (and that admission rule), not a
second GL context.

Compared AEONG_AEONG's Japanese V2 60 FPS/widescreen GameShark file with the
US recomp ([notes](research/jp-60fps-patch-v2.md)). Its extra draw is a
separate game-loop iteration that skips logic and uses the normal flip order;
this is a useful ordering reference for our native render passes. The
single-threaded injected MIPS requires 300% emulator CPU and does not directly
increase worker throughput. It contains a sixth race handler, possibly useful
for Extra Trial, but the US handler is unverified.
No runtime or release settings changed.

Merged upstream #727 and #761 are now approved. Framework fork `r4/main` at
`ab9d181d` backports the final #727 HD pixel fixture, #761's elective HD FMV
timeline and its CD/path repairs, and upstream #769–#772; it also carries the
restore-scope repair from #773. R4 pins this commit. Windows R4 build and 32/32
tests, Mac R4 build and 29/29 tests, nine focused framework checks on Windows,
and nine on Mac passed. A six-second Windows OpenBIOS headless disc run reached
frame 1119 with zero dispatch and segment misses. The HD FMV feature remains
elective; no new performance or retail A/V claim is made. Full upstream master
sync is pending a settings migration: #766 removes the Graphics preset and
launcher fields R4 currently uses; the attempted mechanical merge failed to
build. `docs/UPSTREAM.md` records the exact backports.

The current 0.2.1 **playtest** packages are built from R4 `7063e32` and
framework `ab9d181d`. The Windows ZIP is in
`build-release-prsync-local-20261026/dist/`; all 509 packaged files hash-match
`C:/Users/Shokunin/Games/R4`. The Mac arm64/x64 ZIPs are in
`~/dev/r4-playtest-prsync-20261026/dist/`; all 2,190 arm64 packaged files
hash-match `~/Games/R4-1.0-RC`. Existing settings, inputs and saves were
preserved. The Desktop test shortcuts on both machines are named
`ridge racer type 4 1.0 RC` and point to these installs. The Mac installed
package ran headless for six seconds to frame 1016 with zero unknown dispatch
and segment misses. The Windows installed package booted headless; its
release build has no TCP debug server, so the zero-miss check used the matching
development build above. No tag, upload or public release has been made.

Pre-merge #727 R4 HD/local-view evidence is in
`docs/verification/render-pass-local-view-20261024.md`; its physical output
FPS remains unmeasured. #761's synthetic FMV replay covered 85 HD frames and
pause/load behavior, but retail-content lip sync remains unmeasured. Contributor
#712 still needs an affected-driver compositor reproduction.

Older Desktop review items remain because deletion was rejected by tool policy;
they are not the current test build. Publication is still held for owner review.

[v0.2.0](https://github.com/tetrisgm/RidgeRacerType4Recomp/releases/tag/v0.2.0)
is published for Windows x64 and macOS arm64/x64. The annotated tag points to
`af6e327`; its packaged game source is `b176afa`. The fork sync `75036461`
is on `r4/main`; R4 now pins `c4eb8183`. The published release passed 32/32
Windows and Mac R4 tests and a five-second loaded 60 Hz race with zero dispatch
misses or gaps over 25 ms.

The current Windows install at `C:/Users/Shokunin/Games/R4` is a local
**playtest** package from R4 `70fd703` with framework `c4eb8183`, not a new
public release. All 502 packaged files hash-match the installed copies and 62
existing non-package files, including settings and saves, were preserved
(`analysis/install-playtest-seam-20261024.json`). The earlier published-release
receipts are in `analysis/release-assets-v020-20261024/`. The Mac's existing
dirty R4 checkout was untouched; isolated verification ran in
`~/dev/r4-verify-20261024/r4` and other separate clones.

The installed runtime stayed alive through a five-second headless smoke check;
the desktop R4 shortcut still targets this installation.

Follow-up performance work is sustained output above 60 Hz under shared load.
The 60 FPS extra-frame worker path is the shipped baseline with 30 Hz game
logic. The 90/100/120/144 and Match display choices are available, but a
physical 1440p/120 loaded run averaged 106.5 distinct frames/s, and 144 is
not sustained. Keep OpenGL and PGXP geometry; Ultra enables fine seam closing.
Low retains PGXP geometry. No stereo/VR work is planned. Do not turn local
real-frame offload experiments into release defaults until their state and
timing are exact.

Post-release framework pin `8a3bf9f1` makes postfx use recorded texture
allocation sizes instead of synchronizing GL size queries each output. The hidden GL bloom
fixture passed 156 checks with identical pixels on Windows and Mac, and Mac
built R4. Three five-second loaded 1440p/120 Windows probes had zero dispatch
misses; render busy time was 4.33–4.36 s with the change versus 4.52 s with
`PSX_GL_POSTFX_KNOWN_DIMS=0`. Windows R4 tests passed 32/32 after regeneration
(game C unchanged). Physical 120 Hz stability still needs proof; the public
v0.2.0 release has not been republished.

Framework pin `e1f2e457` tracks the composing GL context's framebuffer
bindings and avoids two driver queries
per queued output. Windows and Mac A/B present-thread pixels matched; the
eight-frame Mac GL frame-generation fixture passed. Short 1440p/120 A/B/A/B
runs reduced measured composition from 2.35 to 2.12–2.20 ms/output, while
overall present counts varied with host load. `PSX_GL_PRESENTER_FBO_CACHE=0`
restores the query path.

Framework pin `6eead0f2` initializes fixed textured sampler uniforms once per
shader program rather than per batch. Windows and Mac
80-frame direct/threaded GL fixtures matched all pixel hashes with the change
on/off. A short loaded 1440p/120 A/B measured replay at 3.97 ms/output off
versus 3.78 ms/output on, with zero dispatch misses; host load varied and
total presentation count did not establish a sustained-rate gain.

Framework `c4eb8183` publishes the copied PGXP seam baseline before GPU
backup and invalidates it on a failed reserve. A five-second same-state race
used 241 prepared seams versus 205 before the change, with fallbacks down
from 124 to 101. The verifier checked 537,523 triangles with zero mismatches;
dispatch misses were zero. Mac async timing passed 53/53 and R4 29/29. This
improves seam-worker readiness; it does not establish a higher sustained FPS.
The pinned Windows build passed 32/32, and a five-second visible loaded 60 Hz
race presented 298 frames with zero dispatch misses (`analysis/pgxp-final-visible-20261024`).

Seam worker diagnostics (`analysis/seam-stats-1440p120-20261024` and an
isolated fallback-reason probe) show every fallback was pending work: 171
pending, zero preflight mismatches in five seconds. Two preparation workers,
incremental chunks, elevated priority, and late binding did not improve
throughput; none was promoted. The ordered PGXP correction remains enabled
when fine seam closing is selected, including the serial fallback. Its
preparation cannot start until the render
thread publishes the baseline at async reserve.

The separate timed real-frame prototype has an exact return-state fix
(`cc59557`) and framework CD-DMA/IRQ edge fix (`d1c2b910`); a 15-second
worker race matched 191/191 stock state-hash ticks with zero dispatch misses.
It still takes about 70 ms wall time per draw across roughly 138 owner
handoffs, so it is not a default performance path. Bounded spin and dirty-page
transport experiments preserved short-run state equality but did not improve
throughput; neither was promoted. A presenter-capacity preflight saved render
work but reduced actual five-second output from 575 to 549 frames at 1440p/120;
it was rejected. Slots frequently free during replay, so any admission change
must reserve capacity without discarding an output that could display.

The remaining high-rate bottleneck is the single OpenGL replay context: CPU
workers generate optional-frame commands in parallel, but the render thread
restores, replays, captures, and rolls back each frame serially. An isolated
renderer-owner refactor (`e04472be`, `2bf7f142`, `5747e92e`) passed focused
Windows/Mac GL fixtures; it has no second context or FPS gain and is not
pinned. HD invalidation read-index commit `0a09d002` improved its microbench
but not displayed frames, so it is not pinned either.

An isolated private-surface replay prototype matched normal, widescreen,
PGXP, and combined PGXP/widescreen fixture pixels on Windows and Mac, but a
five-second loaded 60 Hz comparison showed no output gain (298 versus 299
presents). Its replay finish time rose from 178 to 655 ms total across about
154 optional frames. Full-size scratch color/depth/stencil plus widescreen
targets also need roughly 1.8 GiB at 18x and exceed common texture limits.
Do not pin or install it. The framework already groups ordinary triangle
setup into one render-thread ring record; that item needs no new work.

An isolated two-context fixture proved shared OpenGL textures, ordered owner
readback, and context-private PGXP triangle snapping on Windows and Mac:
12/12 synthetic frame hashes matched serial output. It did not run game replay
or show a credible throughput gain (the RTX synthetic parallel run was slower).
The unpinned owner-state commits are `29ab8f6e`, `70c1b93c`, `e4e8e739`, and
`99562055`; the two-context fixture is `1b5d9df9`. A product implementation
would still need isolated pass/dirty/wide/HD state and immutable command input.
Keep the proven single-context 60 Hz build as the playtest baseline.
The dynamic-resolution controller is already at 5x/1200 internal lines in
loaded runs; its next integer step is 4x/960, below the owner's 1080p floor.
The current receipt reports GPU 15.75 ms versus CPU 5.96 ms and an over-budget
load at that floor. A threshold tweak cannot supply sustained 120 Hz without
breaking the resolution requirement.

## Archived v0.2.0 preparation and investigation

The following status was recorded before the published release and is retained
as a receipt, not as current installation or release status.

The owner authorized the brief display-mode check and the next release, and
asked to remove the recurring approval rule. `VERSION` is prepared as 0.2.0
because v0.1.0 is already public. A 15.031-second visible loaded race ran with
Windows at 2560x1440/120, then the desktop was restored to 7680x4320/60.
The runtime queued 1,803 presentations, with zero late worker frames and
dispatch misses; DXGI capture counted 1,598 distinct screen frames (106.5/s)
under the shared load, so physical 120 FPS is not proven steady
(`analysis/visible-physical-1440p120-20261024`).

One short visible loaded race at a 2560x1440 window with 120 selected ran
10.031 s and queued 1,210 presentations, with zero render-late outputs and
dispatch misses, five timed refusals and one gap over 25 ms
(`analysis/visible-1440p120-20261024`). The SDL game window was foreground
and visible; the owner has not yet commented on its appearance. Windows stayed
at 8K/60, so this checks the 120 output pipeline but not physical 120 Hz scanout.
Short hidden 2560x1440 checks of the same build: 90 selected gave 443
presentations in 5.031 s, 100 gave 508, and 144 gave 453 with 135 render-late
outputs and 33 gaps over 25 ms (`analysis/cadence-1440p{90,100,144}-20261024`).
All had zero dispatch misses. The 144 setting is selectable but overloaded on
this PC under the measured load; do not claim sustained 144 FPS.

The v0.2.0 Windows candidate was built from `eed746b` at
`build-candidate-v020-20261024/dist/r4-0.2.0-windows-x64.zip` (SHA-256
`86D9F0EF1DC522ADA70DDE0737620A5E5BE4D98C9C8B9C9F4BDDD0D8EE4EFCE4`).
The packager's checks passed. It is installed locally at
`C:/Users/Shokunin/Games/R4`; all 499 packaged files hash-match the install,
and ten settings, input, disc-choice, mod-state and save files stayed
hash-identical. The prior installation is backed up at
`C:/Users/Shokunin/Games/R4-dev-backup-20261024-v020`. The installed runtime
started headless for five seconds. This is a local candidate, not a published
release.

The framework upstream-master sync `75036461` is pushed to fork `r4/main`
and R4 pins it. Generated game C stayed unchanged; the R4 build and 32/32
tests passed on Windows and Mac. The framework GL render-thread
fixture passed six pixel modes with zero failures using the local Clang toolchain.
A fresh state was made with
zero dispatch misses, and a five-second loaded 60 Hz race queued 302
presentations with zero gaps over 25 ms or dispatch misses
(`analysis/upstream-sync-60-race-20261024`). Mac verification used an isolated
clone at `~/dev/r4-verify-20261024/r4` because the Mac's existing R4 checkout
has unrelated changes; its build and 32/32 tests passed.

A clean five-second 120 Hz probe at the large default window is
`analysis/gpu-profile-120-20261023`: 409 of 600 requested presents, with
render-thread busy time 5.09 s of 5 s. Window size was the main missing
variable in that result. At a 2560x1440 game window, the *installed*
executable gave 606 presents in five seconds, one timed refusal, zero drops
and dispatch misses (`analysis/installed-1440p120-v2`). A current-build
full-8K 60 Hz hidden-window probe gave 289 of 300 requested presents and zero
dispatch misses (`analysis/current-8k60-v1`). A 3840x2160/60 five-second
probe gave 297 presents, zero late worker frames and dispatch misses
(`analysis/current-4k60-v1`). Windows currently runs the desktop at
7680x4320/60 after the brief physical 120 check. The short results support
1440p/120 output scheduling and 8K/60 under the
measured load, not native 8K/120.

The frame-rate selector now offers Match display refresh while retaining 60
FPS as the default and 30/90/100/120/144 fixed choices. The existing cadence
planner accepts the measured display rate, so this uses the same extra-frame
worker pipeline for standard monitor rates. Defaults check passed, runtime
built, and five-second hidden plus ten-second visible loaded races had zero
dispatch misses. The selector manifest is installed at
`C:/Users/Shokunin/Games/R4/mods/bundled/r4.enhancement.frame-rate/1.0.0`;
its predecessor is backed up in
`C:/Users/Shokunin/Games/R4-dev-backup-20261023-display-refresh`. The 60 FPS
default and installed executable remain unchanged.

Release-candidate boundary (owner, 2026-10-23): the installed 60 FPS
extra-frame worker path is complete for this milestone. Main runs game logic
at 30 Hz; four private workers prepare additional frames in the working mod's
order. The owner saw the race and reported no shaking or out-of-order frames.
The 60 FPS short race, ordered-frame, bloom, seam-identity, project-test and
zero-dispatch-miss checks below passed. This is a development installation,
not a new release.

Real-frame draw offload and sustained 90/100/120/144 FPS are follow-up work,
not blockers for this candidate. Those rates are selectable, but short probes
do not establish target throughput above 60. The isolation gate is sufficient
for the installed extra-frame path; whole-race equality for every mode and
default-mod combination remains unverified. The timed real-frame, presenter
composition and HUD handoff branches below are local checkpoints only.

Current local investigation (2026-10-23 evening): the same OpenGL backend stays.
The opt-in presenter/HUD series was rebuilt from fork `35b3849e` in
`C:/Users/Shokunin/dev/psx-presenter-min` (`c1df89e4`, 24 selected commits).
Its layered OpenGL fixture passed 58/58 and two async final frames matched
direct RGBA bytes exactly. A five-second 144 Hz loaded race admitted 371 full
outputs with zero fallback or dispatch misses, but displayed 374 frames and
had large shared-load gaps; sustained 144 Hz is unproven. It remains local,
opt-in and uninstalled. A follow-up FBO ownership commit `7fa65fbc` deferred
slot reattachment; its fixture passed 60/60 and six display frames matched
byte-for-byte. A five-second 144 Hz probe displayed 370 versus 341 earlier,
with fewer refusals/gaps, but shared GPU load prevents a stable-rate claim.
An isolated flush-order commit `f6375184` passes GL fixtures but its short R4
race fell back on every full-offload attempt, so it has no integrated speed
evidence and is not promoted.
Timed real-frame drawing now commits under a diagnostic
with idle skipping disabled, but differs from stock by 1–7 guest cycles per
draw; the first live pad/SIO Timer1 sample is ten counts early. This is a
correctness stop, not a value to mask. Trace receipts are in
`C:/Users/Shokunin/dev/r4-timed-audit/analysis/timed-audit-v4`. The isolated
serial-cycle guard `13ece33` refuses promotion on the first -1 draw. A
deadline-only IRQ prototype then failed owner DMA-state reconciliation at
`0x80066654`. A narrow isolated fix recognized device-raised `I_STAT` while
still checking DMA registers; two course draws then committed, but the next
draw reached the cycle guard 25 cycles early. The prototype is not pinned or
installed.
The combined presenter/resource and GL batching prototypes are isolated in
`C:/Users/Shokunin/dev/r4-hud-handoff` (framework submodule `93218aed`), not
pinned or installed. Five-second 144 Hz A/B/A results varied with shared GPU
load (281/301/306 displayed), so they establish no throughput gain; a 60 Hz
loaded race stayed near 60 with zero dispatch misses or late worker frames.
Focused hidden GL fixtures found exact color/stencil parity for opaque bloom
fusion, depth-state query removal, and skipped implicit source staging. The
last also matched final pixels for normal, fallback and leased async presents.
The full-width wide-batch shortcut failed raw-wide pixel equality and was
abandoned. Existing reprojection stays off: it changes timing and previously
showed visible race artifacts. An early-expiry replay rollback passed its GL
fixture and skipped 13–25 expired replays per five-second run; fast/legacy/fast
display counts (352/311/315) are too noisy for an FPS claim. A refreshed
timed real-draw probe passed its old device deadline; generic scratchpad
`LWC2` support (`493998b3`) then advanced it to an IRQ handoff refusal before
any GPU packets. The generic two-checkpoint fixture and rejection diagnostics
(`7580e02b`, R4 adapter `e45715d`) pass; they did not reproduce the R4 IRQ
refusal, whose exact invariant needs one fresh diagnostic probe. See
`analysis/hud-ab-*` in the local integration checkout and
`C:/Users/Shokunin/dev/r4-timed-audit/analysis/timed-audit-v3/probe/runtime.log`.

Released: v0.1.0 only. R4 master contains the developer-only isolation gate
(PR #65) and four extra-frame draw workers (PR #69). The source fork pin is
`35b3849e` (expired async replay rollback); the development build installed at
`C:\Users\Shokunin\Games\R4` has SHA-256
`173E18CC49D98DDBF34BD5C99858BE0FFFA70F20DB11F4581DAAE86D2B19D7A4`.
The prior executable is backed up at
`C:\Users\Shokunin\Games\R4-dev-backup-20261023-async-expiry`. Settings,
saves and bindings were preserved. This is not a release or tag. The new pin's
visible 10-second race check is `analysis/async-expiry-visible-v2`: 302 worker
jobs admitted, 601 extra-frame presentations queued, zero late workers and
dispatch misses; 599 present-thread displays with six timed drops under shared
load. Windows R4 tests pass 32/32 and the Windows/Mac GL rollback fixture
matched color and stencil pixels.

The game still simulates at 30 Hz on main. Four private contexts prepare the
working 60 FPS mod's exact in-between draw sequence; 60 is the default and
30/90/100/120/144 remain selectable. A fresh master-build race state was made
headless in bounded stages (`analysis/merged-prep-v99`, slot 2, zero dispatch
misses). The installed master executable then ran visibly for 15 seconds:
`analysis/worker-master-visible-v100` recorded 900 distinct ordered frames,
no gap over 25 ms, refusal, render lateness or dispatch miss. Windows project
tests pass 32/32; Mac tests available without the disc pass 29/29. Full 8K
private-draw pixel identity is in `analysis/worker-pixels-v13`.
After the seam pin, `analysis/seam-master-visible-v6` ran visibly for ten
seconds: each second after startup showed 60 distinct frames, with 302 worker
jobs admitted, zero drops/failures and zero dispatch misses. Required
regeneration added 14 private draw entry hooks; the short exact-draw receipt
`analysis/seam-pin60-v5` passed. Mac built the pin and passed 29/29 tests.

Performance checks must set `PSX_DISPLAY_RING=0`; the debug ring otherwise
reads full VRAM every game frame and understates visible throughput. Corrected
short hidden-window checks reached 90 Hz (271 distinct frames in the last
three seconds), but 100/120/144 fell short (289/330/358). Four workers beat
2/3/6/8 at 144 Hz on this eight-core PC; the render thread is the limit.
The HD invalidation indexing experiment (`perf/hd-invalidation-index`) remains
unmerged because it did not show a reliable end-to-end gain.
The queued-pass seam worker cut render replay from 6.96 to 4.11 ms in a
five-second 144 Hz A/B on the shared PC; distinct output rose from 66.8 to
96.8/s, still below target. A three-second 60 Hz comparison matched 113,120
triangle seam results bitwise, with zero mismatches or dispatch misses. The
owner saw no shaking or frame-order issue in the visible race. The R4 pin
build, 32/32 Windows and 29/29 available Mac project tests, and a
three-second slot-2 probe passed with zero misses. Postfx/driver presentation, live VRAM packing and
texture work dominate the remaining render cost. Earlier post-effect
capability and viewport caching showed no measurable gain and remain local.
Fork `990ffb00` also fixes queued-frame bloom source provenance; previously
the configured effect ran zero times in a short 60 Hz race. The corrected
build ran it 181 times in three seconds (about 0.57 ms/pass) and retained
60 distinct frames/s in steady seconds, with zero worker failures or dispatch
misses. Windows and Mac hidden GL fixtures passed 151 checks each. The fix is
pinned and installed as a development build.

Follow-up: complete real-frame draw offload only after private execution preserves
guest timing, IRQ/device service, mod hooks and call-point effects. The gate's
sound/RNG handoff is already synchronous on main; PRs #71/#72 reject false
passes. The local generic timed-context topic (`C:/Users/Shokunin/dev/psx-real-timing`,
`feat/timed-draw-checkpoint`) and R4 adapter (`C:/Users/Shokunin/dev/r4-timed-adapter`,
`feat/timed-real-draw-adapter`) now run the Grand Prix track call through
private timing, OpenBIOS, IRQ/device and mod-hook handoffs. The latest short
diagnostic reached private scratchpad and then a device deadline; no complete
real-frame call or equality proof exists, so this remains opt-in and unmerged.
The build-compatible headless race state is `analysis/timed-real-fresh-v18`;
short refusal receipts are `analysis/timed-real-trial-v19` onward. A separate
GPU GP0 IRQ1 class fix and callback DMA hold fix are local framework topics,
not installed. The installed development build remains the visually approved
60 FPS extra-frame worker path, now pinned to `35b3849e`.

For higher rates, local `C:/Users/Shokunin/dev/psx-present-compositor`
(`feat/async-present-composition`) has immutable presenter slots for color,
mask, depth, OSD and source images, plus separate GL resource pools. A short
loaded race had 150/150 worker admissions, zero dispatch misses and zero
ordering mismatches; postfx composition still runs on the render thread.
The companion generic HUD prepare/replay API and R4 packet adapter are in
`C:/Users/Shokunin/dev/psx-hud-handoff` and
`C:/Users/Shokunin/dev/r4-hud-handoff`, with pixel parity still unverified.
Presenter assessment: local checkpoint `cfb53ddf` fixes borrowed mask/depth
texture reinitialization. A 6-real/15-output bloom fixture passed ordering and
effect-job checks; equal source pixels produced equal final pixels. Generated
sources varied even between baseline runs, so whole-game parity is unproven.
R4's host HUD currently disables this opt-in offload, and it moves only the
roughly 0.57 ms bloom pass, so it cannot yet close the 144 Hz throughput gap.
The HUD handoff builds, but renderer slots do not carry HUD packets and exact
HUD pixel parity is unverified. These local branches remain disabled,
uninstalled and unmerged; see the branch's
`docs/ASYNC_PRESENT_COMPOSITION.md`. Keep
performance samples to 15 seconds and automated checks to 30 seconds.
References and sequence: `docs/CODEX_PLAN.md`.

## Owner decisions

- Display: widescreen Fit, Match display, supersample 1.5, FXAA off, dynres
  floor = 1080p, render + present threads, Smooth motion off (removed,
  owner 2026-10-20), PGXP depth buffer / colour correction / fine seams. All in `game.toml`.
- `guest_cycle_scale = 2` (races, Max Detail gate).
- HD HUD bundled, on by default, credited to Kuid0us.
- Online: 2-4 players, each on their own full-screen view.
- Rewind off in split screen and in every online session.
- Release model: bundled compiled zips (#18). Version: TBD.
- Graphics presets: low-end machines start Low (extras off); others Ultra
  with dynamic resolution, frame rate and aspect.
- Modern controls: NeGcon analog gas/brake/steering, forgiving curves.
- New launcher: tetrisgm/recomp-launcher replaces recomp-ui, minimal R4 menus.

## Blockers

- Gate: the stock Grand Prix passed 18,101 isolated ticks, but Time Attack,
  split-screen tails, after-goal replay and default-mod whole-race equality
  remain unverified. The latest default-mod Time Attack bounded check matched
  252 ticks and 504 input polls. See `docs/DRAW_ISOLATION.md`.
- Threading: real-frame draw still runs on main. The lean contexts used for
  optional frames omit the guest cycle/IRQ model, so they cannot replace it
  until timed private execution and call-point side-effect handoff are proven.
- Throughput: 100/120/144 Hz have ordered outputs but miss target on this
  shared PC at the 1200-line dynamic-resolution floor. Profile render work
  before changing worker count or output timing.
- Release: owner authorized the next release on 2026-10-24; v0.2.0 is the
  prepared version after the existing public v0.1.0.
- Online battle keeps its experimental switch until psxrecomp reserves link memory per netplay session.

## References

- Disc identity: `DISC.md` (Redump 11608).
- Framework/UI pins: `docs/framework_pin_history.md`.
- Widescreen: `docs/WIDESCREEN.md`; cull lists `tools/r4_ws_scan.py`.
- Display defaults: `README.md` (On by default); `tests/test_r4_defaults.py`,
  `tools/check_pin_keys.py`.
- HD HUD: `docs/HD_HUD.md`, `tools/r4_hd_hud_pack.py`.
- Internal resolution presets: `README.md` (Internal resolution), `tools/res_matrix.py`.
- Release: `tools/package_release.sh` (gates: OpenBIOS only, HD HUD pack exact, pin keys).
- Verification loop: `tools/run_r4.sh build` + `tools/smoke.py <out-dir>`;
  rollback determinism: `PSX_RB_SELFCHECK=1 PSX_RB_SELFCHECK_MASH=1`.
- A/B guest identity (mod off, warm vs cold shards):
  `psxrecomp/tools/fp_identity.py` with
  `--launch 'tools/run_r4.sh {build} --debug-port {port} {headless}'`.

# Phase 5: native per-frame rewind

Implementation on `feat/frame-arch-5-rewind`, based on native-authority
`4f893c1a`. Framework code is title-independent. The supervisor integrates;
this topic incorporates the packet-presenter branch through `27d6d167`.

## Enhanced reverse playback and restore repair (2026-10-29)

The earlier native-only presentation limitation below is superseded. Immutable
frame packets, including owned native texture provenance, are now retained and
associated with native capture ticks. Holding rewind restores each machine state
and submits its original packet through the normal enhanced presenter. The next
restore waits for presentation completion. At the common packet/snapshot history
boundary it holds the oldest available image; release discards the future,
reseeds forward presentation and resumes play. Audio stays muted while held and
its timeline resets on release. No selection menu or launcher change is needed.

### Cause and repair

The duplicated HUD was a composite ownership error: after native reseeding, the
fast widescreen center splice treated the native 4:3 center as equivalent to the
enhanced wide image. This copied centered native HUD pixels over repositioned
enhanced HUD pixels. Restore now invalidates this equivalence for the renderer
session and uses full mirroring. It also invalidates presentation backup/pass
generations, depth, seam, hold/interpolation and HD texture caches. GPU restore
reapplies texture-window, draw-offset and mask state; successful machine restore
invalidates PGXP shadow metadata without disabling any PGXP feature.

The renderer change is limited to restore invalidation and its fast-path guard;
the packet presenter implementation is inherited from the main agent. A hidden
GL regression deliberately creates different native/enhanced centers, proves
the old splice corrupts the center, and verifies restore preserves the enhanced
center and all PGXP settings. Its packet-history test replays owned texture
provenance after destroying the original native producer.

### Memory and integration seam

The retained packet ring has a 512 MiB accounted byte cap, including allocated
seed/record vector capacity and attachment payloads, plus small container and
shared-pointer allocation overhead. This cap is unchanged at 8K: history stores
CPU draw records and native texture provenance, never 8K framebuffers or GL
readbacks. Native snapshot storage separately reserves about 368 MiB. Existing
live mailbox slots and presenter GPU allocations are additional: up to three
leased/pending/producer packets can survive history eviction (128 MiB seed and
attachment limit, 32 MiB record limit per packet, with vector capacity overhead).
The 512 MiB number is a history budget, not total process or GPU memory.

`frame_packet_history_*` owns retention, byte eviction and shared storage
lifetimes. `rt_history_mark(tick)` records the packet published at a capture;
`rt_history_available/ready/present` enforce aligned replay and completion;
`rt_history_branch(tick)` drops the future. Restore-only draw records stay private
and are discarded during reverse playback. Replay uses a fresh epoch/sequence
with immutable original packet storage. Evicted but leased storage remains live.
Oversize packets may present normally but cannot be rewound past the common
history boundary. No redraw preroll is needed.

Merged `origin/feat/frame-arch-2-mailbox` through `27d6d167` (including the phase-3
owned-input work). Native-raster merge conflicts retain phase-5's fixed draw queue
and ordered checkpoint mailbox, adding the packet branch's texture observers.
The supervisor should preserve that checkpoint protocol during subsequent queue
integration. Owner defaults remain Rewind, 60 FPS, Car Effects and PGXP fully on.

### Final validation

Release build and 31 applicable CTests pass. The legacy PGXP-off boot reference
test is excluded under the owner's instruction. Regeneration leaves all 50
shards unchanged. The final hidden-GL packet/restore fixture passes 111 checks.
The central Beads path `F:/Software/beads/issues` is unavailable on this PC;
no replacement local database was created.

Native ordering/texture-input, immutable mailbox/history, hold controller and
portable snapshot handoff fixtures also pass. Snapshot checks include compressed
and raw images, rejected-input atomicity, and PGXP-shadow invalidation on success.
Fresh hidden boot-to-race smoke completes all 40 steps through frame 9,759 with
zero dispatch/segment misses; captures are in `build/rewind-packet-final-smoke`.

Fixed 8K test on Ryzen 7 9800X3D / RTX 4080 SUPER, hidden window and dummy audio:
120 seconds, 7,200 captures, zero frame gap/failures/dispatch or segment misses.
Mean emulation-thread capture cost **0.200516 ms**, p50 **0.196 ms**, p99
**0.316 ms**, maximum **0.7186 ms**. Worker cost totals 2,464.3093 ms across the
run (0.3423 ms/capture on its separate CPU thread), maximum 0.8625 ms. The mean
meets 0.5 ms, but this run does not establish a strict sub-0.5 ms worst-case
guarantee. No outliers were discarded. All PGXP features and Car Effects remain
on; the Effects counter advances 7,202 times. Resolution was pinned via
`dynres_force scale=18`: `internal_lines=4320`, `hr_scale=18`, FBO 18432x9216,
hidden drawable 5120x2880. A preceding unpinned run also passed (mean 0.197645 ms,
max 0.4733 ms), but is not the fixed-8K timing proof.

Mapped host Y: **94 restores, 94 byte-exact verifications, 94 enhanced packet
replays**, zero misses, closed menus, successful forward resume. Debug hold:
**272 restores/verifications/replays**, zero misses; a 12-second hold reaches
the packet budget boundary and the last 30 samples hold the same frame, then
release resumes. Observed packet history peaks at **511.8862 MiB**. Screenshots
capture the actual enhanced presentation owner, not native scanout.

Local ignored evidence (game images are not committed):

- `build/rewind-packet-fixed8k-120s.json`: complete timing/counters/settings.
- `build/rewind-packet-final-host.json` and `build/rewind-packet-final-debug.json`:
  per-sample machine state, byte verification counts, menu probes and capture
  completion receipts. Reproduce with `tools/rewind_demo.py --port 4798 --host
  --visual build/rewind-demo --out build/rewind-demo.json`; omit `--host` and add
  `--hold-seconds 12` for the debug-held history-boundary check.
- `build/packet-held-small.png`: before-fix doubled rank, record, map and tach.
  `build/rewind-packet-final-host/held-8-small.png` and `released-small.png`:
  clean single HUD during hold and after release; full-resolution originals
  accompany these inspection thumbnails. Forward and second held captures are
  also preserved in both demo directories.
- `build/rewind-packet-gl/rth-om51menb/receipt.json`: hidden GL fixture, 111 checks.

## Follow-up: latency and enhanced reverse acceptance

Historical follow-up: this was **not phase-5 acceptance**. Enhanced reverse
presentation was missing; the native-scanout reverse path below was in use. The earlier
implementation and measurements are retained below as historical evidence.

### Allocation and capture ownership

The delta journal now allocates and faults in a 256 MiB circular byte pool,
32 MiB current image, and 32 MiB maximum reverse-edge scratch at configuration.
A separate 32 MiB immutable capture image is also preallocated. CPU/device
serialization copies directly into that image; mod arenas stream their exact
wire representation without an intermediate arena-sized copy. Module/bitmap
scratch is fixed and prefaulted (16 MiB). These buffers reserve about 368 MiB,
separate from the native raster queue and other runtime memory. Retained used
bytes reported by `rewind_status` are not this allocation budget.

Native completion tokens are reused, including fixed storage for a partial
CPU-upload fragment. The native job deque is replaced with a fixed-capacity
ring, retaining the existing byte/admission limit. The single outstanding
rewind checkpoint uses a separate preallocated mailbox tagged with the next
native draw sequence. The consumer applies it after all earlier work and
before any later work; publication takes no draw-queue mutex or capacity wait.
A retained event prevents lost wakes. On Windows it uses an auto-reset kernel
event instead of the C++ condition variable's internal mutex or timed polling.
Additional independent checkpoint clients can still use ordinary queue
backpressure. The serializer emits the same machine bytes as before. After the
native checkpoint completes, the emulation thread publishes its immutable GPU
and VRAM spans to a dedicated CPU worker. That worker fills the reserved spans,
compares 4 KiB pages, constructs predecessor deltas and retires old ring metadata.
It neither reads GL nor accesses changing guest RAM. History becomes visible
only after acquire/release completion publication. No buffer is reused while
the worker owns it; discard and shutdown drain it before releasing storage.

All delta allocation/free and eviction copying are removed from the emulation
thread. Eviction does not free memory even on the worker. Byte-budget pressure
can retire additional oldest edges instead of allocating more storage. Readiness
checks are coalesced over at most 64 basic-block polls, with frame/input requests
bypassing the coalescing. The guest keeps executing while a checkpoint is pending.
Capture telemetry reports emulation-thread work; `worker_ms` and `worker_max_ms`
report the separately scheduled comparison/publication work, without hiding it
inside the native raster budget. The delta-worker handoff also publishes one
preallocated job with atomics and retained wake/completion events. Windows
capture submission has no C++ mutex or condition-variable operation.

### Measurements collected during this follow-up

AMD Ryzen 7 9800X3D (8 cores/16 threads), RTX 4080 SUPER. Release with debug
tools, hidden window, dummy audio, port 4798 and PGXP fully enabled. Owner mods were enabled in the manifest. The first three runs exposed
a separate registration bug: session activation cleared the shared HUD/Car
Effects callback, so Car Effects did not actually draw. Those runs are not
all-effects acceptance evidence. Both HUD and Effects activation now rebind
the shared callback; subsequent soaks require a positive `r4.effects.draw`
counter delta. Each row is a complete 600-second measurement; no outlier was
removed. Percentiles are upper edges of 1 microsecond histogram bins; maximums
are exact wall-clock observations, including preemption. The 9x runs used the
title's display-resolution default. Race slot 8 was reloaded during the runs to
avoid the race time limit; the scripts retain those events in their receipts.

| Implementation | Internal scale | Captures | Mean ms | p50 ms | p99 ms | Max ms |
|---|---:|---:|---:|---:|---:|---:|
| Pool and reusable native token | 9x | 36,035 | 0.429798 | 0.380 | 0.966 | 31.8575 |
| Plus streaming arenas/coalesced polling | 9x | 35,946 | 0.286240 | 0.268 | 0.571 | 2.2097 |
| CPU worker, Effects callback absent | 18x | 35,979 | 0.198128 | 0.191 | 0.312 | 0.9607 |
| CPU worker, Effects callback repaired | 18x | 35,884 | 0.199853 | 0.192 | 0.330 | 6.1421 |
| Plus stage/thread-cycle diagnostics | 18x | 35,844 | 0.201376 | 0.193 | 0.340 | 4.5909 |
| Mailbox with timed polling (rejected) | 18x | 29,932 | 0.382894 | 0.257 | 1.023 | 3.5118 |
| Final retained events + scoped priority | 18x | 35,994 | 0.185991 | 0.178 | 0.298 | **0.5175** |

The final run **passes the capture gate**: 600.062 seconds, 35,994 captured
frames across exactly 35,994 frame boundaries, zero capture failures, zero
dispatch/segment misses and 35,997 Effects draws across the surrounding counter
probes. Begin maximum is 0.5112 ms, completion maximum 0.0251 ms and readiness
maximum 0.0006 ms. CPU-worker comparison/eviction costs 0.282802 ms mean and
0.9602 ms maximum, separately from the emulation-thread figures above. Receipt:
`build/rewind-event-8k-10min.json`. This is a measured workload result, not a
hard real-time guarantee or acceptance of enhanced reverse playback.

The first two rows fail the strict maximum-under-2-ms gate. The second run's
begin maximum was 1.9625 ms, completion maximum 0.4138 ms, and readiness maximum 0.0009 ms.
The third row passes capture timing but lacks working Effects; its CPU-worker
maximum was 7.7395 ms, off the emulation thread. The fourth row has 35,884
Effects draws, no capture failures and no dispatch/segment misses, but fails
the wall-clock maximum. Its begin maximum is 6.1310 ms, completion maximum
0.1106 ms, readiness maximum 0.0073 ms and worker maximum 1.2382 ms.

The fifth run retained the slowest begin stage breakdown: 4.3539 ms in native
checkpoint submission, 0.1385 ms in CPU/RAM copying, 0.0532 ms in SPU work and
0.0322 ms in remaining devices. That capture consumed 1,226,696 Windows
thread-cycle units versus a 2,995,673-unit maximum over the run; these units
are processor-dependent, not converted to elapsed time. The evidence localized
the stall to the queue submission path. The mailbox change removes both its
queue mutex and condition-variable wake from the capture path. Wall-clock
maximums remain authoritative; scheduling time is never subtracted.

The timed-polling experiment lost 6,048 captures across 35,980 frame boundaries
and is rejected independently of its 3.5118 ms maximum. The final event-based
implementation has no periodic worker polling. The soak now checks capture
count against the frame-counter delta (allowing two in-flight edge samples).

That experiment also exposed a 3.4533 ms RAM-copy stage using only 812,950
thread-cycle units, consistent with host descheduling. While rewind is
configured, Windows now raises a normal-or-lower emulation thread to
`THREAD_PRIORITY_ABOVE_NORMAL`, without lowering an already higher priority.
Shutdown/reconfiguration restores the prior priority. This matches the existing
presentation worker's priority class and is not real-time scheduling.

All 18x runs used internal 4320 lines with a hidden 5120x2880 drawable, dynamic
resolution off and no scale clamping. The race save is a car at a barrier,
reloaded every four minutes; these are repeated race-state measurements, not
full-course driving. The earlier rows use the original thread priority. The
final event-based version uses the scoped emulation-thread priority above;
process priority and CPU affinity remain unchanged.

Local receipts: `build/rewind-pool-10min.json`,
`build/rewind-pool-stream-10min.json`, `build/rewind-worker-8k-10min.json`,
`build/rewind-final-8k-10min.json`, `build/rewind-profile-8k-10min.json`, and
`build/rewind-mailbox-8k-10min.json`.
The effects-enabled host-Y demo in
`build/rewind-effects-host-proof.json` captured 601 frames at 0.211549 ms mean,
restored 124 byte-exact states, kept the main menu closed on all 40 probes and
resumed capture after release.

Final event-based demos also pass: `build/rewind-event-host-proof.json` records
602 new captures at 0.202902 ms mean and **151 byte-exact mapped-Y restores**;
`build/rewind-event-debug-proof.json` records 600 captures at 0.189674 ms mean
and **154 byte-exact debug-hold restores**. Both resume forward capture on
release; all 80 combined main-menu probes remain closed. No rewind selection
image is emitted by the default overlay ABI.

### Verification

- `tools/regen.sh` (JOBS=4): 50 shards unchanged; no generated edits.
- Release build and 31/31 applicable R4 tests pass. The legacy PGXP-off boot
  fixture is excluded under the owner's PGXP-always-on rule.
- Delta fixture covers allocation-free hot operations, byte-budget/depth
  eviction, wrap, variable-length payloads, abort, backward steps and branching.
- Native fixture covers pixel oracles, partial-upload checkpoint ownership,
  40,000 additional queue-wrap writes, two interleaved checkpoint clients,
  exact later-write exclusion and worker destruction with pending completion.
- Serializer fixtures cover deferred patch lifetimes (including the portable
  byte-order path), native/raw wire identity and streaming mod-arena identity.
- Worker fixture covers publication, concurrent-submit rejection, 1,000 reuses
  and shutdown draining; held-rewind fixture covers safe boundaries, mute,
  reverse order, release, branching, netplay/title gates and absent menu image.

### Enhanced playback: remaining implementation

The selected integration design is retained, immutable presentation packets
aligned with snapshot IDs/epochs, carrying their own initial native texture
seed, complete draw state, PGXP vertices, uploads/copies, display configuration
and title effect metadata, including post-processing uniforms and host overlays.
Restore should submit the corresponding packet to the ordinary enhanced renderer, then resume from the native snapshot on release.
Future packets must be discarded on the new timeline. That preserves resolution
and enhancement settings without reading GL into a machine snapshot.

This branch does not yet provide that integration. Its legacy `RtCmd` ring is
incremental and contains control/debug/pass records with lifetimes unsuitable
for arbitrary replay. Recording that ring verbatim is not an immutable packet
implementation. A facade-only recorder would also omit projected primitives,
texture-bank selections and effect metadata. Blind replay would not establish
normal-play visual fidelity. The renderer plumbing in `gpu_gl_renderer.c` has
therefore been left untouched, as requested.

Composed-output QA is also not a visual pass. The local
`build/rewind-event-forward.png`, captured after the final hold demos, shows
duplicated stock HUD content over the barrier scene. Reloading the race save
changes that composition (`build/rewind-event-reloaded.png`). These observations
are retained as presentation defects; byte-exact native restoration does not
prove enhanced-image identity. No renderer change or visual-parity claim is
included in this follow-up.

An uncompressed image ring is not selected: one 7680x4320 RGBA image consumes
126.56 MiB; 750 images consume 92.70 GiB before depth or intermediate surfaces.
A 2 GiB ring would retain only 16 images (about 0.27 seconds at 60 Hz). Reducing
resolution or silently falling back to native output would miss the owner gate.
The independent packet integration must be completed with the phase-2/4 renderer
boundary before claiming enhanced rewind is delivered.

### Reproduction

After starting a hidden race, use:

```
python tools/rewind_soak.py --port 4798 --seconds 600 --reload-slot 8 --out build/rewind-soak.json
python tools/rewind_demo.py --port 4798 --host --out build/rewind-host-proof.json
```

The soak exits unsuccessfully if the capture maximum reaches 2 ms, the mean
reaches 0.5 ms, capture fails, Car Effects never draws, dispatch/segment misses
occur, or capture count falls behind the frame-counter delta. `--reload-slot`
is optional and must name a local race save. The host demo uses mapped Y, checks
that the runtime main menu remains closed, compares every restored machine byte,
and verifies resumed forward capture after release.

## First-pass state and lifetime (historical)


Every guest frame requests capture at the next resumable basic-block edge.
CPU registers, RAM, scratchpad, device state, clocks, instruction-cache tags,
executable-RAM bitmap and mod memory stream directly into a reverse-page journal.
Changed 4 KiB pages retain their predecessor bytes; unchanged pages are shared
through the current image. Content comparison covers DMA, HLE and direct-pointer
writes without introducing write-barrier overhead or omitting untracked writes.
Variable-length device FIFOs retain their previous lengths across rewind.

GPU registers and partial-command state are captured on the guest thread. An
ordered CPU-native checkpoint captures VRAM and stale-but-valid texture/palette
cache entries at that same command-stream position. Guest execution can continue
while it completes. The snapshot stays unpublished until both sections have
been patched into its journal. Polling, patching and publication costs are
included in the capture counter. Destruction/discard waits for CPU completion
before freeing destinations. No snapshot reads GL or enhanced render targets.

The legacy 50-snapshot preference now retains 750 every-frame edges, about
12.5 seconds at 60 Hz; the old sparse interval preference no longer reduces
capture cadence. The ring evicts oldest edges and discards the future as the
user rewinds. Storage in the measured race was roughly 95 MiB, rather than
750 full machine copies. A disk-state load or disc change discards history.

## Interaction

Hold F8, the configured host shortcut, or Y in R4 Modern controls. Each restored
native scanout is presented at a nominal 60 Hz host cadence. Release re-enters
the scheduler at the restored PC. No guest instructions execute between reverse
steps. At the oldest retained frame, holding keeps that frame displayed.
Audio is paused during reverse playback; old host PCM and resampler history are
discarded, with a fade into resumed output. PGXP configuration stays enabled.
Enhanced output is disposable: normal rendering reseeds after release.

There is no filmstrip, selection cursor or accept-to-load action in the default
path. The old overlay ABI remains inert for compatibility. Netplay and the
existing title block (R4 split screen) still refuse rewind. No launcher source
change was needed: input and the former filmstrip were owned by the runtime.

## Measured evidence

Windows, AMD Ryzen 7 9800X3D, GCC 16.2 Release with debug tools, worktree-local
build, hidden OpenGL window, `SDL_AUDIODRIVER=dummy`, port 4798. Rewind, 60 FPS,
Car Effects, geometry/texturing/CPU-mode/culling/depth/colour/seam PGXP defaults
were enabled. No PGXP-off reference was run.

| Race capture window | Captures | Mean host capture cost |
|---|---:|---:|
| TCP hold demo | 605 | 0.381678 ms |
| Additional window 1 | 601 | 0.406631 ms |
| Additional window 2 | 587 | 0.407000 ms |
| Additional window 3 | 585 | 0.427349 ms |
| Final rebuilt TCP demo | 601 | 0.467880 ms |

These are measured emulation-thread costs for begin, readiness polling, delta
patching and publication. Native raster work is asynchronous on its CPU worker.
They are averages, not a hard real-time maximum: the longer validation run
observed a 26.18 ms capture outlier, including allocation/host scheduling.
This does not assert phase-2 guest pacing or phase-4 enhanced interpolation
acceptance. Debug-disabled performance was not separately measured.

The TCP demo verified 122 consecutive restored full serialized states byte for
byte, then resumed capture. A separate host-input Y hold/release test verified
another 52 states (174 total), with no restore mismatch or dispatch/segment
miss. The final rebuilt demo independently verified another 124 restores.
Tests check the live restored machine, not merely the stored delta bytes.

Component checks: reverse delta eviction, variable-length tail, fragmented
streaming, abort rollback, branch-after-release, delayed native completion,
muted reverse presentation, safe-edge gating, netplay/title gates, raw/streamed
serializer identity and atomic load rejection. Native raster fixtures also
check asynchronous queue-position identity and completion after worker teardown,
including texture-cache state and the existing pinned native pixel fixtures.

R4 CTest: 31/31 passed. `r4_pgxp_boot` is excluded because that legacy test
explicitly runs PGXP off. Live all-on debug checks replace that portion here.
Boot-to-race smoke: zero dispatch and segment misses at every scripted step.

## Reproduce

Build in an isolated checkout with `JOBS=4`, Release and `PSX_DEBUG_TOOLS=ON`.
Start with `SDL_AUDIODRIVER=dummy`, `--hidden-window --no-launcher
--debug-port 4798 --memcard-dir build/rewind-saves`. Supply the worktree's game
config/disc and its own overlay compiler as in `tools/run_r4.sh`.

```
python tools/smoke.py build/rewind-smoke --port 4798
python tools/rewind_demo.py --port 4798 --out build/rewind-proof.json
```

`rewind_status` returns frame, captures, restores, failures, retained bytes,
cumulative capture time, maximum, and serializer phase totals. The debug-only
`hold:1/0` drives the same reverse loop; `verify:1` reserializes each restored
machine and compares every byte against its snapshot. Verification runs only
while rewinding and is excluded from capture measurements. For the real mapped
button path use `set_input` with `layer:"host", buttons:"0xEFFF"` (Y), then
`buttons:"0xFFFF"` to release. `screenshot` selects native scanout during rewind,
since the ordinary enhanced wide surface then belongs to an abandoned timeline.

Local proof artifacts are under `build/rewind-*.json` and
`build/rewind-final-smoke/`. Game images and saved states remain untracked.

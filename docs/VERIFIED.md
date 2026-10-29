# Verified-state ledger

Feature -> last commit whose code touched it -> evidence. A feature whose
code has not moved since its row is not re-tested (CLAUDE.md, Verification
scale). Update the row when its code changes or a new check is run.
Evidence dirs are local (not tracked).

## Frame architecture phase 2 component history, 2026-10-28

This historical checkpoint predates the production cutover and acceptance
measurements below. At that point only the components were integrated.

| Component | Result | Evidence |
|---|---|---|
| Callback-owned audio resampler/SPSC PCM, MMCSS | Exact waveform/controller parity; concurrent wrap and timeline resets pass; old push-audio switch removed | `audio_transport_test` |
| Owned frame packets | Immutable seeds, bounded replacement, overflow rejection and load epochs pass | `frame_mailbox_test` |
| Native packet seed | Newest packet reproduces exact native pixels after 62 pending packets are replaced; observation leaves rewind dirty rows intact | `native_raster_test` |
| Presentation-owner worker | 10000 publications complete while consumer callback is blocked; only startup/shutdown context transfers; timed work continues without new guest packets | `frame_mailbox_test` |
| Producer draw-state mirror | Six hidden GL modes retain matching pixels; extra-pass state is restored before the next real frame | `analysis/perf-20261028/phase2-state-pass-gl/rth-rk999nru/receipt.json` |
| Native HD provenance ownership | Original GP0 write observation, masked partial upload/copy keys, dropped-frame sample usage and shared immutable pixel snapshots pass; production binding makes native authoring independent of presentation | `gpu_hd_context_test`, `duckstation_texture_pack_test`, `native_raster_test` |
| Durable owner requests | 64 owned requests survive 9998 frame replacements while consumer stalls; load invalidation completes stale requests; idle queries complete without guest frames | `frame_mailbox_test` |
| Per-draw native texture inputs | Shared native page/CLUT tiles retain pre-write versions; wrapping, restore lifetime, metadata sharing, overflow recovery and 62 dropped-packet inputs pass | `native_raster_test`, `gpu_hd_context_test` |
| Texture banks and input identities | Six hidden GL modes retain parity; bank selection takes no context; source ID survives producer advance; bank generation on/off retains identical real images with full renderer PGXP | `phase2-native-input-source-gl/rth-pq5xrw76`, `phase2-bank-framegen/receipt.json` under `analysis/perf-20261028/` |
| Native provenance integration | Dev build, 32/32 R4 tests and hidden HD GL fixture pass; smoke has zero dispatch/segment misses, live/paused diagnostics zero context acquisitions; owned PID 46044 stopped | `analysis/perf-20261028/phase2-native-provenance-runtime/` |
| Native input/bank integration | Dev build and R4 32/32 pass; hidden smoke and live/paused queries have zero misses/acquisitions; PID 39400 stopped. Registered bank replay test passes | `analysis/perf-20261028/phase2-native-input-runtime/`, `gl_frame_gen_bank_test` |
| R4 integration | Dev build and 32/32 tests pass; hidden smoke reaches race with zero dispatch/segment misses; live/paused diagnostic queries retain zero context acquisitions | `analysis/perf-20261028/phase2-state-runtime/` |

Remaining phase-2 integration: independently seeded GL packets, durable
configuration/diagnostics, exact native HD provenance, retained-surface handling
after packet replacement, and the slowed-GPU real-time acceptance run. Phases
3, 3b, 4 and 5 remain unimplemented. Phase 4 retains the owner's 100% match bar.

## Native-authority phase gate, 2026-10-28

Framework `464bc104`: native GP0/GPUREAD/snapshot fixtures pass; the all-on
18x race records **zero context acquisitions/releases**. Queue backpressure
is still present and belongs to phase 2. This is not real-time acceptance.
Legacy non-race ownership APIs and transaction deletion remain, as recorded
in FRAME_ARCHITECTURE's sequencing note.

| Change | VBlanks/s | Max interval ms | Starved samples | Swap completions/s | Scene / present GPU ms |
|---|---:|---:|---:|---:|---:|
| Native history + queued diagnostics (`native-phase1-history18`) | 53.075 | 67.360 | 79173 | 53.16 | 3.670 / 12.813 |

15 seconds, hidden 7680x4320,18x,60 FPS requested,Rewind/Car Effects/full
PGXP on. Distinct images/s is not established by swap counts. Total GPU
rolling average16.523ms; aggregate emulation CPU13.421ms is not per-thread
attribution. Cumulative render-queue backpressure10604.621ms; no context moves.

Full standalone framework build passed. Non-hardware CTest:297 listed,
2 disabled,3 skipped,292 passed after repairing stale fixture links and
three stale source/fixture guards. The external-runtime diagnostic test then
passed live and paused against an owned hidden process (12 queries including
FBO readback/diff and native display history), with zero acquisitions.
Dev build/R4 CTest32/32 passed. Six hidden GL fixture modes match all direct/
threaded pixel hashes, including Release configuration publication and an
immutable image exceeding half the transport ring. Hidden tools/smoke.py
reached race frame8767 with zero dispatch/segment misses; the race screenshot
was inspected. Native history was enabled for this smoke.

Receipts:local analysis/perf-20261028/framework-unit-results.txt (initial
run plus passing targeted rerun),native-history-gl-gate/rth-oybouali/,
native-native-history-gate/,native-phase1-history18.json. All owned processes
were stopped. No visible window or display-mode change occurred.

## Frame architecture native integration, 2026-10-28 — phase 1 open

Production original-GP0 capture, native reads/scanout and cache-preserving GPU
snapshot wire are integrated on the phase-1 topic. Dev build and R4 ctest
32/32 pass. Ten focused framework parser/native/scanout fixtures pass, as does
actual-parser debug/production parity with partial-A0 save/load. Hidden
boot-to-race smoke has zero dispatch and segment misses. Separate pinned GPU
oracle: 10000 mixed draws, 10000 coordinate cases, 2000 interlaced draws and
2000 fill/copies match every native halfword; PGXP remains enabled.

Same-save hidden 18x, 60 FPS requested, Rewind/Car Effects/full PGXP on:

| Build/change | VBlanks/s | Max interval ms | Starved samples | Swap completions/s | Scene / present GPU ms |
|---|---:|---:|---:|---:|---:|
| Earlier all-on baseline (`fundamentals-60-18`) | 27.309 | 143.778 | 362296 | 26.04 | not recorded here |
| Queued snapshot copy (`fundamentals-copy18`) | 40.043 | 124.752 | 220180 | 40.07 | not recorded here |
| Native authority integrated (`native-authority-integrated-18`) | 39.716 | 77.948 | 224724 | 39.69 | 6.312 / 14.502 |
| Queued controls / private presentation mirror (`native-owner-18`) | 9.643 | 251.139 | 566583 | 10.15 | 15.418 / 75.359 |
| Repeat with GPU telemetry, 10 s (`native-owner-memory18`) | 43.031 | 123.660 | 128764 | 43.85 | 9.300 / 12.022 |
| Large-record transport and ownership handshake (`native-owner-final18`, `1f5f6e1a`) | 53.353 | 64.265 | 78556 | 53.33 | 3.720 / 12.625 |

These are 15-second diagnostic runs, not Release acceptance. Swap completions
are **not** distinct-image counts. GPU figures are the renderer's rolling
256-sample averages. Native integration removes guest GPU readback dependency;
it does not yet remove queue backpressure or the old presentation passes.
Phase 1's zero-GL-wait gate remains open; no phase has been promoted.
The three owner-queue runs recorded zero context acquisitions/releases over
the whole run, but still incurred render/present queue backpressure. Their
large variance is unresolved; these moving-race samples are not sufficient
to attribute the entire cadence change to a code change. The 10-second repeat
used about 11040 MiB total GPU memory (including other desktop use), with
2790 MHz graphics clock and up to 98% GPU utilization.

`1f5f6e1a` is pushed as WIP, not merged or pinned. Build and 32/32 R4 tests
pass; the six hidden GL modes match direct/threaded hashes, including an
immutable CPU image larger than half the 32 MiB command ring. The transport
fixture passes five repeats of 8860 checks after fixing a stale ownership
acknowledgement. Hidden `tools/smoke.py` reaches race with zero dispatch and
segment misses (`native-owner-final-gates/`). Additional diagnostic ownership
work remains uncommitted and is not covered by that measurement.
Receipts are local `analysis/perf-20261028/native-integrated-gates/`,
`native-field-gate/`, `native-transfer-gate/`, and the named JSON files.

## Earlier component checkpoint, 2026-10-28 — phase 1 incomplete

Framework topic `feat/frame-arch-1-native-authority`, WIP `b56f4914` plus
`5ca01d0e` and native-polygon WIP `643af69d`, is pushed to `fork` but not merged or pinned. Independent raster
contexts and the native CPU command queue are implemented; runtime GP0 routing
is not switched. The older deferred-Rewind branch is preserved separately.

| Phase | Acceptance result | Verification so far |
|---|---|---|
| 1: native authority | **Open**: no runtime zero-GL-wait claim | Native worker fixture passes ordered writes, copied uploads, mask/CLUT/wrapping, concurrent context isolation, dirty masks, snapshot/restore and partial A0 upload resume. Existing hidden GL fixture passes all six modes with matching threaded/direct hashes. Dev build passes; R4 ctest 32/32; framework `native_raster_test`, `boot_state_handoff_test`, `gpu_sw_edges_test` pass. Hidden `tools/smoke.py` reaches race with 0 dispatch and 0 segment misses. |
| 2: mailbox/audio | Not started | No GPU-slowdown acceptance result |
| 3: surfaces | Not started | No new GPU-frame-time result |
| 3b: PGXP audit | Not started | Owner amendment incorporated into the implementation plan |
| 4: generic packet interpolation | Not started | No primitive-match-rate or 60/120-image result |
| 5: rewind | Not started | No per-frame delta-snapshot or hold-to-rewind result |

Receipts: `analysis/perf-20261028/native-context-gl-fixture/rth-e6vaorfh/`,
`native-phase1-gates/`, and the native fixture target in
`psxrecomp/build-prsync/`. The full dev build/32 tests/smoke were run after
`b56f4914`; the subsequent dirty-tracker adapter/assertion adjustment was
checked by rebuilding and running the native fixture. No architecture
before/after performance row is reported: the native worker is not yet wired
into the game, so timing the existing race path would not establish phase 1.

Integration finding: existing `gr_draw_*` inputs can already contain
widescreen/HUD transforms and RGB555 truncation. Exact native capture must
precede those changes. The existing native SW implementation also has colour
interpolation/dither limitations recorded in `psxrecomp/accuracy/axis5_gpu.md`.
These remain correctness work, not a passed hardware-accuracy gate.

`643af69d` adds a separate native-only RGB888 triangle input, integer attribute
planes and native dithering. Its gradient and all 16 dither-position assertions
pass in `native_raster_test`; `gpu_sw_edges_test` also passes after removing
undefined signed shifts for negative edge coordinates. This new path is not
yet wired into runtime GP0 decoding, and has not passed full oracle parity.
The earlier dev-build/smoke results do not constitute validation of that
integration.

## Owner PC performance, 2026-10-28

DESKTOP-BAKIEMJ, Ryzen 9800X3D / RTX 4080 SUPER, 7680x4320 60 Hz.
Hidden real-GL window with an asserted 7680x4320 client, same GP grid save
(slot 9), Modern RT throttle, turbo off, 25 seconds per row. PGXP geometry,
perspective, CPU propagation, depth, colour and fine seams all remain on.
Car Effects is on in every row below. Dynamic resolution is off.

| Configuration | Main VBlank callbacks/s | Max callback interval, ms | Pacer debt >= one period, entries | Starved audio samples | Distinct presents/s |
|---|---:|---:|---:|---:|---:|
| Previous defaults: 18x internal, Rewind on, Auto (30 FPS) | 50.898 | 105.680 | 853 | 171184 | 25.48 |
| Same, only Rewind off | 56.738 | 44.896 | 409 | 68502 | 28.40 |
| 9x internal, Rewind off, optional Auto (60 FPS) | 53.049 | 60.156 | 949 | 124948 | 26.72 |
| New defaults: 9x internal, Rewind off, original race frames | 60.049 | 41.599 | 0 | 0 | 30.04 |

Evidence: local `analysis/perf-20261028/{acceptance18-before,acceptance18-no-rewind,final30-effects,final30-effects-verified}.{json,present.csv}`.
The `final30-effects` filename predates checking its activation log: it is
the **60 FPS** row, confirmed by `r4.framerate: auto -> 60 FPS`; the
`-verified` run explicitly disables the frame-rate feature. No dispatch or
segment misses in any row. Distinct images use changed 8x8 presenter sample
hashes within the timed window, a lower bound rather than a physical-display
capture. Audio counts are missing source **samples**, not underrun episodes.
Main callback intervals include work and pacing; pacing debt is not a count
of lost guest IRQs. The final run had no entry a full VBlank period late.

Debug captures/timers that do not ship are off: `PSX_DISPLAY_RING=0`,
`PSX_PRESENT_IMAGE_RING=0`, `PSX_GL_PERF=0`,
`PSX_GL_ASYNC_GPU_TIMER=0`, `PSX_GL_ASYNC_GPU_OPS=0`.
`PSX_PRESENT_TRACE` alone is enabled for the distinct-image receipt.
Earlier exploratory runs with those diagnostics enabled are not acceptance
measurements. No visible game window or display-mode change was used.

Causes: Rewind performs synchronous full-machine/VRAM snapshots on the
emulation thread; 18x rendering also exceeds real-time capacity. At the
existing 9x Match display cap, extra race draws still overload 8K output.
Car Effects requires serial extra draws; disabling it restores workers but
only reached roughly 44-48 presents/s at 8K output. Original frames reached
stable 30 FPS with Car Effects retained. Rewind and the frame-rate enhancement
are therefore opt-in. This is **not** a claim that 60 FPS has been restored.
CPU-mode-off and seam-off probes did not cure the remaining slowdown;
controller enumeration is event-driven, and closed overlay/capture handling
was not the cause. PGXP and its subfeatures keep their shipped defaults.

The optional worker path also fixes a reproducible rational-clock bug:
elapsed-window and excessive-lead skips previously drifted its boundaries
by nanoseconds, requesting near-one duplicate draws. Exact indexed endpoints
are tested against the framework output planner at 60/90/100/120/144 Hz,
after stalls and lead skips, over a long skipped span, and with PAL timing.

Final pin `ac65fcf5`: regeneration succeeded with all 50 shards, the
2994-entry dispatch table and 618 draw-context clones unchanged. Build and
32/32 tests passed. One hidden smoke reached race frame 8494, all 40 steps
with zero dispatch/segment misses. Receipts: `analysis/perf-20261028/final-gates/`.
Four obsolete auxiliary CMake caches referencing a removed toolchain were
reversibly renamed `CMakeCache.stale-20261028.txt`; the shared `build/` remains
configured. The debug command's Release guard was corrected after smoke;
its preprocessed debug implementation is identical, and build/ctest reran.

Fresh shipped-default recheck (no settings or feature overrides),
`acceptance-defaults`: actual drawable 7680x4320, effective internal scale 9,
dynamic resolution off, PGXP/CPU/depth/seams on. 60.037 main callbacks/s,
38.710 ms maximum interval, zero starved audio samples and 30.00 distinct
presents/s. This image-traced repeat had 128 over-period pacing entries
(49.449 ms maximum debt), so the earlier zero-lateness run is not a promise
that debug readback cannot perturb scheduling. Clean repeat with the image
trace disabled, `acceptance-defaults-no-trace`: 59.977 callbacks/s,
41.630 ms maximum interval, **zero** starved samples, **zero** over-period
pacing entries (maximum debt 9.380 ms), 30.00 presents/s and zero presentation
hitches. Composed screenshots of defaults and optional workers were inspected
(`acceptance-defaults.preview.png`, `worker-visual-check.preview.png`);
the race scene and HD HUD render correctly. The worker visual check is not
a performance acceptance run.

| Feature | Verified at (R4 / psxrecomp) | Evidence |
|---|---|---|
| Modern controls: analog gas/brake/steer, digital fallback, rumble | 12ba3f4 / ab9d181d | `tools/test_modern_controls_runtime.py` 8/8; `C:/Users/Shokunin/dev/r4-vinput/run` |
| Rewind (1P on; VS split screen blocked) | 12ba3f4 / ab9d181d | `splitscreen` test 8/8, same dir |
| Camera look-around | 12ba3f4 / ab9d181d | `r4-vinput/run/abc/ov_*.png` |
| JogCon compat | 12ba3f4 / ab9d181d | `tools/test_jogcon_runtime.py` |
| Widescreen Fit 16:9 / 21:9 (GP, TA, VS) | 12ba3f4 / ab9d181d | `C:/Users/Shokunin/dev/r4-vdisp/analysis/vdisp/vis-default-60b`, `vis-219-force18` |
| 60 FPS races (59/s, max gap 35 ms, loaded PC); 30 steady | 12ba3f4 / ab9d181d | vdisp `vis-fps-clean2`, `vis-rate30` |
| Race frame rate Auto: 60, original 30 from internal scale 18 (8K) | pin PR / e887344e | fork render_pass_motion_test; `tests/test_r4_defaults.py`; R4 32/32; headless smoke to race-12s (frame 9073) 0 dispatch / 0 segment misses with the log line `r4.framerate: auto -> 30 FPS (internal scale 18)` on the 8K PC; on-screen 18x not run (owner playtest) |
| Max Detail, no pop-in | 12ba3f4 / ab9d181d | vdisp `vis-slot8-maxdetail/burst_*.png` |
| HD HUD | 12ba3f4 / ab9d181d | vdisp `hud_*_1to1.png` |
| PGXP + seam snapping at 18x (strips only where the neighbour is: no kerb / lane-mark bars) | pin PR / 9d93976c | `C:/Users/Shokunin/dev/r4-pgxp/analysis/pgxp/evidence` (tick-exact attract-demo captures, before / after / snapping off), `runs/async-verify2` (prepared seams mismatch 0, 394,170 triangles verified), `runs/smoke2` headless 0 misses |
| Dynamic resolution | off by default since 249b5c3 (owner) | — |
| Dynres climbs at 8K/60 (CPU cost, queued-output cost, cadence guard); worker output-grid lead bound | pin PR / e4efba40 | `C:/Users/Shokunin/dev/r4-glctx/analysis/k8d-*`, `k8g-new45w` (one on-screen run), fixtures `scratchpad/psx-build/fwctest.log`; settle level on a quiet panel unverified |
| Hide rear-view mirror | 12ba3f4 / ab9d181d | vdisp `mirror_compare.png` |
| Projection replay (framework PSXDrawReplay) for R4 extra frames | tried on 243eca3, reverted: 33.4 vs 113.3 distinct/s at 1440p/120, +30% CPU per pass | HANDOFF 2026-10-27 |
| Render-thread CPU per extra frame (HD tracking miss path) and presenter admission wait | pin PR / 0bcc5a49 | `C:/Users/Shokunin/dev/r4-glctx/analysis/rt-*`, `phys-*`; GL fixtures `scratchpad/glfix-final` |
| GL fixtures (18) and present_thread_test on the fork | pin PR / 0bcc5a49 | same |
| Extra Trial widescreen | not verified (3f493ea reverted) | — |
| Windows package | df285a0 | `analysis/release-assets-v021-20261027` |
| macOS packages (arm64, x64) | df285a0 | same |

## Frame architecture phase 2 ownership continuation, 2026-10-29

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

Native provenance capture and packet attachment lifetime are prerequisites only;
production GL remains on the incremental ring. Phase 2 acceptance is pending.

Framework WIP `cb0dde2d` is pushed on the phase-2 topic, unmerged/unpinned.
Registered `gl_hd_texture_pack_test` also passes (both hidden HD modes plus
its native-texture baseline, PGXP renderer features enabled for all three).
Native-context tests cover both Beetle and Duck formats.

## Complete-packet GL gate, 2026-10-29 (phase 2 WIP)

Framework topic `feat/frame-arch-2-mailbox`, WIP `50ff71f9`, remains unmerged
and unpinned. The registered `gl_frame_packet_test` uses a hidden real 3x GL
context with PGXP depth, colour perspective and seams enabled. It replays actual
RTH records through the new owner, restores complete producer draw state,
replaces 62 pending packets while the consumer is stalled, and consumes retained
texture versions after the native worker is destroyed. All 71 checks pass;
context transfers occur only at startup/shutdown. This is a synthetic ownership
and pixel test, not a race throughput measurement.

| Gate | Result | Evidence |
|---|---|---|
| Complete packet GL replay | 71/71; two consumed frames, 62 replacements | `analysis/perf-20261028/phase2-packet-replay-gl-final/rth-grkmih5g/receipt.json` |
| Timeline reset / optional batches / concurrent owner | Pass, 20 repeated runs | `frame_mailbox_test` |
| Parser binding and history guard | 2/2 pass | `gpu_render_pass_provenance_test`, `gpu_gp0_history_guard_test` |
| Dev build and R4 tests | Pass; 32/32 | `ninja -C build -j2`, CTest |
| Hidden runtime smoke and diagnostics | Zero dispatch/segment misses; 12 live + 12 paused queries; zero context acquisitions/releases | `analysis/perf-20261028/phase2-packet-owner-runtime/`; owned PID 24552 stopped |

The production GL path still uses the incremental ring. No phase-2 acceptance,
performance improvement, framework promotion or R4 pin bump is claimed.

## Frame architecture production packet cutover, 2026-10-29 (WIP)

Framework `feat/frame-arch-2-mailbox` at `1d14aef6`; not merged to r4/main,
R4 gitlink unchanged. Every race below uses a hidden 7680x4320 window, 18x,
60 FPS race setting, Rewind and Car Effects enabled, full PGXP. Swap completion
is not evidence of distinct images. No dispatch or segment misses in these runs.

| Change / evidence prefix | VBlanks/s | Maximum ms | Starved samples | Swap completions/s | Verdict |
|---|---:|---:|---:|---:|---|
| Phase-1 `native-phase1-history18` | 53.075 | 67.360 | 79173 | 53.16 | Blocking presentation ring |
| Packet cutover `packet-cutover-timers18` | 60.124 | 41.642 | 0 | 51.23 | Guest average real time; final target unmet |
| 50 ms owner stall `packet-cutover-stalled18` | 57.653 | 50.350 | 11869 | 14.14 | Phase-2 stress gate fails |
| Nonblocking present `packet-nonblocking-present-stalled18` | 58.871 | 50.307 | 9643 | 14.57 | Gate still fails |
| Native batch experiment `packet-native-batch-stalled18` | 58.697 | 52.430 | 12493 | 15.09 | No demonstrated benefit; reverted |

Evidence is under `analysis/perf-20261028/`. Render queue/context waits are
zero in packet runs; remaining CPU cost still matters. Native batch experiment
was not retained. The later SPSC native submission experiment is not included
in `1d14aef6` and requires its own result.

Component correctness: `render_thread_test`, `frame_mailbox_test`, and the
updated nonblocking `present_thread_test` pass. Complete-packet hidden GL:
`phase2-cutover-packet-gl/rth-69hl3f1d`, 71 checks. Full-PGXP bank interpolation
endpoint parity: `phase2-cutover-fg-bank-ack/fg-h9bvoh1y`, four cases at 3x,
489/509 checks per case, identical real-image digests with generation on/off.
The latter explicitly acknowledges real endpoints in the test so intentional
mailbox replacement cannot erase comparison samples; drop correctness remains
covered by the independent complete-packet fixture.

Further phase-2 WIP gates (not phase acceptance): native SPSC queue preserves
all oracle/pixel fixtures over 20 repeated tests and passes an explicit blocked
consumer/full-queue wrap/order test. Pending-mailbox admission now sheds obsolete
optional guest redraws; the 50 ms stall run still fails real time (58.645 Hz,
46.863 ms maximum, 10648 starved; `packet-pending-admission-stalled18`).

R4 32/32 passes. Hidden smoke exposed and then verified removal of two guest
GL-readback paths: missing-wide presentation fallback and widescreen screenshots.
`phase2-cutover-capture-runtime` passes smoke and live/paused diagnostics with
zero misses/acquisitions. `phase2-startup-live-scale-runtime` also verifies owner
startup before guest execution and paused live allocation through 720p, 1440p,
and 8K, preserving byte-identical native screenshots. Owned PIDs 9212 and 31012
were stopped. Full-PGXP GL bank endpoint parity and owner-allocation callback
checks pass in `phase2-owner-scale-fg-bank/fg-v4akfmyc` (495/515 checks per case).

### Frame architecture phase 2: active-effects and stalled-owner correction (2026-10-29)

Earlier phase-2 race receipts enabled Car Effects in the mod plan but did not
execute its draw callback. Session activation clears the host HUD callback;
R4 registered the shared HUD/effects layer only in a process constructor.
Both plugins now register it during activation. Prior all-on descriptions are
configuration records, not proof that effects ran. The harness now rejects a
measurement unless `r4.effects.draw` increases; `tools/test_car_effects_runtime.py`
checks both draws and consumed race metadata.

| Change / receipt | VBlanks/s | Max interval ms | Starved samples | Swap completions/s |
|---|---:|---:|---:|---:|
| Effects activation repaired (`packet-effects-active18`) | 59.359 | 53.539 | 7630 | 44.76 |
| Current-deadline optional-work admission, normal (`packet-deadline-normal18`) | 59.983 | 43.919 | 0 | 55.07 |
| Current-deadline optional-work admission, owner delayed 50 ms (`packet-deadline-stalled18`) | 60.093 | 41.906 | 0 | 16.37 |

Both runs: hidden 7680x4320 window, 18x, configured 60 FPS/Rewind/Car Effects,
full PGXP, zero dispatch/segment misses and zero context acquisitions/queue
waits. The slowed-owner run passes the phase-2 cadence/audio condition, not the
final distinct-image or maximum-interval target. Swap counts are not distinct
image counts. The old optional guest redraw remains until phase 4; admission
now refuses when its measured cost cannot fit the current guest deadline,
instead of spending historical idle credit while the guest is already late.
R4 32/32 and six focused native/queue/present/pass tests pass.

Phase-2 acceptance receipts also include `phase2-deadline-effects-runtime`
(hidden smoke through frame9728,12 live and12 paused diagnostic queries,
720/1440/4320 live allocations with identical native PNGs) and
`phase2-capture-mark-runtime` (two asynchronous paused marks,120 effect draws
in the two-second check). All had zero dispatch/segment misses. Full framework
build passed; non-hardware CTest:300 listed,295 passed,2 disabled,3 skipped.
Framework topic27d6d167 passed the phase-2 cadence/audio gate and was merged
locally. Fork integration also imports the independently landed PGXP audit
0e97c44d; its combined build/GL checks are in progress before the pin is pushed.
The phase-3 surface conversion and phase-4 transaction deletion remain open.

Integrated fork pin `a8a62e54`: imported PGXP CPU regressions5/5 pass,
`phase2-merged-pgxp-origin/rth-whf7lxvs` passes real-GL translated-page tests
at1x/4x/9x with all corrections on, R4 32/32 passes. Required regeneration
completed with no generated changes. No BIOS/recompiler regeneration needed.

## Frame architecture phase 3 groundwork, 2026-10-29 (not accepted)

On `feat/frame-arch-3-surfaces`, commit `98ff69cb` (pushed, not merged): scissored depth clears replace clear geometry;
flat/textured/upload-quad streams replace per-batch orphaning with bounded
fenced storage, persistent on the owner PC. GL3.3 fixed-storage/subdata uses
the same lifetime contract. Packet GPU timestamps exclude inter-packet idle.
The full-VRAM surfaces, duplicate submission and colour/depth replay remain.

- Dev build and R4 CTest 32/32 pass. Framework full build and 295 non-hardware
  tests pass; the one stale generated TCP command index was regenerated and
  its check rerun successfully.
- `phase3-stream-wrap/rth-32_w5d30`: 4110 pixel/storage checks pass across
  persistent and portable implementations, with 31 flat and 102 textured
  segment rotations per run.
- `phase3-stream-pgxp-fixed/rth-3wmh6we9`: translated pages, analytical colour
  and perspective texture coordinates, depth probes and scissored depth
  preservation pass at 1x/4x/9x, with PGXP enabled throughout.
- `phase3-stream-packet/rth-_765jj_9`: 74 checks pass, including exactly two
  GPU-timed consumed packets after 62 pending replacements.
- `phase3-new-profile-smoke`: hidden boot-to-race smoke to frame9754, zero
  dispatch/segment misses. Owned PID7452 stopped. The execution identity
  changed from50425fc7 to c3745900 after framework integration, so slot9 is
  correctly rejected. Preserved slot9; created fresh fixture in unused slot10.
- Hidden18x, actual7680x4320 client, Rewind,60config, active CarEffects and full
  PGXP: `phase3-stream-clear-slot10-18`, owned PID15980 stopped. Scale remains18,
  no dynamic scaling; zero dispatch misses and guest context/queue waits.

| Receipt | Fixture | VBlanks/s | Max interval ms | Starved samples | Swap completions/s (not distinct images) | GPU packet elapsed ms | Presentation portion ms |
|---|---|---:|---:|---:|---:|---:|---:|
| phase2 packet-deadline-normal18 | slot9, old execution identity | 59.983 | 43.919 | 0 | 55.07 | not validly scoped | not validly scoped |
| phase3-stream-clear-slot10-18 | slot10, new execution identity | 60.044 | 41.345 | 0 | 51.20 | 18.948 | 12.509 |

This is not a same-savestate speedup comparison. GPU values are timestamp
elapsed intervals for776 consumed packets, not utilization; they may include
submission bubbles inside a packet. No telemetry capacity drops. Distinct
image rate is not established. Final performance and phase3 acceptance remain
unmet; continue the surface redesign before phases3b/4/5.

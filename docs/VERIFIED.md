# Verified-state ledger

Feature -> last commit whose code touched it -> evidence. A feature whose
code has not moved since its row is not re-tested (CLAUDE.md, Verification
scale). Update the row when its code changes or a new check is run.
Evidence dirs are local (not tracked).

## Frame architecture implementation, 2026-10-28 — phase 1 incomplete

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

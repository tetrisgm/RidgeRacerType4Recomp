# Handoff

## Current work: frame architecture, 2026-10-28 (phase 1 incomplete)

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

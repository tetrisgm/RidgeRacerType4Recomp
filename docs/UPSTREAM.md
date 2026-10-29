# Upstream ledger

R4 pins `tetrisgm/psxrecomp` `r4/main` (CLAUDE.md, fork model). Every change
that fork carries over upstream master is listed here with its upstream PR.
When this table is empty, re-pin to upstream master and archive the fork.

Architecture phase-2 topic `feat/frame-arch-2-mailbox` through `27d6d167`
is merged and pushed to fork `r4/main`, integrated pin `a8a62e54`. The remote
had independently advanced with PGXP audit `0e97c44d`; those changes are retained.
No upstream or R4 PR was opened or merged. Phase3b acceptance will be rechecked
after the surface conversion, rather than inferred from the imported audit.

Architecture phase-1 topic `feat/frame-arch-1-native-authority` through
`464bc104` is merged to fork `r4/main` at `4f893c1a`. No upstream or R4 PR was
opened. Native-authority verification and the remaining ownership/packet work
are recorded in VERIFIED and FRAME_ARCHITECTURE. The old queued-VRAM-snapshot
branch remains unmerged; native CPU snapshots supersede its authority path.

The latest full upstream-master sync is `d108e9a0` (2026-10-24). The current
fork pin `e4efba40` (on `0bcc5a49`, below) additionally backports merged #761, #769–#772, #727's
final pixel fixture, and #773's restore-scope repair. Full master sync was
tested locally but cannot be promoted as a pin bump: upstream #766 removes
Graphics presets and launcher settings still used by R4. A mechanical merge
failed the R4 build. The fork retains those controls while their replacement
is designed and verified. Windows R4 32/32 and Mac R4 29/29 tests pass on the
targeted pin; generated game C is unchanged. The fork-only changes below remain.

| Change | Topic branch | Upstream PR | State |
|---|---|---|---|
| Complete-frame mailbox, immutable native inputs, SPSC audio/MMCSS, nonblocking presentation and async controls/captures | `feat/frame-arch-2-mailbox` | none | in fork `r4/main` via `a8a62e54`; phase-2 slowed-owner cadence/audio gate passes |
| Translation-stable PGXP raster/attributes, precision provenance and audit counters | independently landed fork audit | none | retained from `0e97c44d`; revalidate after phase3 |
| Native authoritative VRAM, original GP0 capture, queued renderer controls and diagnostics | `feat/frame-arch-1-native-authority` | none | in fork `r4/main` via `4f893c1a`; phase-2 backpressure remains |
| Opt-in main VBlank wall cadence and pre-wait pacing debt (`main_cadence`) | `fix/race-cadence-metrics` | none yet | `ac65fcf5`; live hidden-window diagnosis, no guest timing changes |
| Live internal resolution, Windows borderless popup, pixel labels | `fix/display-playtest-20261028` | none yet | `a7122c59`; existing display playtest fixes carried by the performance pin |
| Prefer Steam's active virtual pad over a silent physical Valve pad | `fix/prefer-steam-virtual-pad` | none yet | `487ec042`; carried by the performance pin |
| Keyboard seat adopts a pad; Steam desktop-layout echo guard for physical Valve pads | `fix/steam-controller-outside-steam` | none yet | local (`80b058e6`), not pushed; needs hands-on Steam Controller check |
| Avoid postfx GL texture-size driver queries for known renderer surfaces | `fix/postfx-known-dims` | none yet | in `r4/main` (`8a3bf9f1`); exact GL pixels on Windows and Mac |
| Avoid composition-context GL framebuffer-binding queries per queued output | `fix/presenter-fbo-bindings` | none yet | in `r4/main` (`e1f2e457`); exact present-thread pixels on Windows and Mac |
| Initialize fixed textured sampler uniforms once per shader program | `fix/gl-sampler-uniforms` | none yet | in `r4/main` (`6eead0f2`); exact direct/threaded GL pixels on Windows and Mac |
| Publish the immutable PGXP seam baseline earlier during async reserve | `fix/seam-early-publish` | none yet | in `r4/main` (`c4eb8183`); more prepared seams consumed, exact PGXP verifier and Mac async timing passed |
| Dynres: hold through savestate load | `r4/main` commit `ade1ecd3` | none yet | needs topic branch + PR |
| Transactional render passes (60 fps) | `feat/render-pass-hd` | #727 | merged upstream; final HD pixel fixture backported to `r4/main`, full master sync pending #766 migration |
| Queued render passes, 60 FPS pacing (docs/RENDER_PASSES.md) | `feat/render-pass-queued` | none yet | in `r4/main` |
| MinGW test link fixes | `fix/mingw-test-link` | none yet | in `r4/main` (+2 r4/main-only commits) |
| PGXP seam snapping (gaps sub-pixel positions open at crests / T-junctions) | `fix/pgxp-tjunction-seams` | none yet | in `r4/main` (`3c2e76f5`) |
| Lazy seam normals (pure optimization of the fork's seam snapping) | `fix/pgxp-snap-lazy-normals` | none yet (depends on seam snapping) | in `r4/main` via fork PR #1 (`c8641551`) |
| Queued-pass seam preparation on a persistent worker (depends on seam snapping and queued passes) | `feat/pgxp-seam-batch-prep` | none yet | in `r4/main` via fork PR #2 (`0421f31e`); ordered fallback remains serial when late |
| Queued-interpolation postfx source mask/depth provenance | `feat/queued-postfx-provenance` | none yet | in `r4/main` (`990ffb00`); restores configured bloom on generated frames |
| Host threading: cached GL blend/stencil state, batched render-thread ring, disc read-ahead worker (docs/HOST_THREADING.md) | `feat/host-threading` | none yet | in `r4/main` (`ba0a839e`) |
| Threaded draw contexts, lean clones, OpenBIOS qualification and bounded worker pool | `feat/threaded-contexts` | none yet | in `r4/main`; private draw verified at 60 Hz |
| Draw-isolation gate (debug only) | `feat/draw-isolation-gate` | none yet | in `r4/main` via `6f8b4621`; R4 PR #65 merged |
| Timestamped async output, ordered presentation, depth/seam checkpoints and private GPU replay | `feat/output-timestamp-plan`, `feat/async-render-admission`, `fix/async-output-grid`, `fix/async-presentation-ownership`, `fix/async-untracked-copy` | none yet | in `r4/main`; pixel/ordering receipts in `docs/HANDOFF.md` |
| HD provenance leases and async backup transfer | `fix/hd-tracking-generation`, `perf/async-backup-transfer` | none yet | in `r4/main`; PC/Mac component checks and R4 race probe |
| Bounded presenter capacity and per-title slots | `feat/bounded-present-capacity` | none yet | in `r4/main`; eight slots at R4's 8K budget, direct fallback on allocation failure |
| Dynamic-resolution variable-work verification and optional floor start | `fix/dynres-variable-output-work`, `feat/dynres-start-floor` | none yet | in `r4/main`; R4 selects the 1080p floor start |
| Presenter cadence diagnostics and OpenBIOS stamp refresh | `feat/present-cadence-stats`, `fix/openbios-stamp-current` | none yet | in `r4/main`; stamp checked on PC/Mac |
| Windows render-thread scheduling under host load | `fix/render-thread-priority` | none yet | in `r4/main` (`688380ef`); `PSX_RENDER_THREAD_PRIORITY=0` disables it |
| Abort already-expired async GPU replay and roll back its private transaction | `fix/async-expiry` | none yet | in `r4/main` (`35b3849e`); Windows/Mac GL pixel and stencil parity, short R4 race with zero dispatch misses |
| GL fixture harness repair: shared stubs, `run_gl_render_thread.py --compare-env` | `fix/gl-fixture-harness` | none yet | in `r4/main` (`c6c0c62b`); all 18 GL fixtures pass on the PC |
| Exact texture modulation in the accurate blend basis ((texel8*colour8)>>7; was *2/255, +1/255 on modulated texels) | `fix/gl-fixture-harness` | none yet | in `r4/main` (`54369270`); mask-mode scale fixture, blend-accuracy oracle 0 mismatches |
| HD texture-pack tracking: allocation-free miss path, index-driven invalidation test, CLUT hashed only for replaceable candidates | `perf/render-thread-cpu-hd-tracking` | none yet | in `r4/main` (`dfc39ed6`, `78ce6940`); identical render-thread digests, R4 HD_NOTE replay 944 -> 71 ms per 15 s |
| Presenter: wait up to `PSX_PRESENT_ADMIT_WAIT_MS` (25) for a slot before refusing a timed output | `perf/render-thread-cpu-hd-tracking` | none yet | in `r4/main` (`0bcc5a49`); present_thread_test 123/123, R4 refusals 62-200 -> 3 per 15 s |
| Dynamic resolution: CPU cost (GPU span excluded), queued-output cost, output-cadence guard; async GPU timer diagnostic | `feat/dynres-gpu-busy` | none yet | in `r4/main` (`e4efba40`); R4 8K/60 climbs 5 -> 13x, fixtures pass |
| Caller-chosen render-pass rate activation and the resolved internal scale as mod API (`psx_mod_activate_render_pass_fps`, `psx_mod_internal_scale`) | `feat/render-pass-fps-internal-scale` | none yet | in `r4/main` (`e887344e`); render_pass_motion_test; R4 Auto rate (30 at 18x) |

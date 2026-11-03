# Upstream recovery and owner preview

This is a development recovery on `recovery/upstream-20261102`, published to a
remote topic branch at the owner's request. It is not a release or a claim that
every fork enhancement has been recovered. Default branches remain unchanged.
Start with [the collaborator rebuild handoff](COLLABORATOR_HANDOFF.md).

Follow-up work from the owner's campaign playtest is tracked in the
[feedback burndown and goal instructions](FEEDBACK_BURNDOWN.md): distant
scenery culling, widescreen steering-camera comfort, high-resolution rewind
and, last, Tomba-style resident loading. The owner excluded an 8K slowdown
investigation. Those four outcomes remain unvalidated.

On 2026-11-03 the owner reviewed the 4K internal-resolution / 8x anisotropic
setup and said it looked pretty decent. That accepts the owner preview for
this recovery; it does not qualify the separately parked performance topics.
The portable settings are in [approved-4k.toml](approved-4k.toml), applied to a
built runtime by `tools/apply_recovery_4k.py`. The helper's preservation,
backup, repeat-application and invalid-schema checks pass; four existing static
configuration checks also pass. Runtime code and submodule pins are unchanged
from the owner-reviewed build, so that playtest was not repeated for this
documentation/profile handoff.

## Frozen identities

| Component | Identity |
|---|---|
| Original game | `14d798191852` / `archive/original-20261102` |
| Original engine | `84d6ca22259ad16d9dd5e13f390514e575cb8d10` / engine `archive/r4-original-20261102` |
| Common engine ancestor | `d19d8c7ef468577ede65b0b9cc4ee8fbce17717d` |
| Recovery engine | upstream `c1f5cafd4102daa57b7342bec7c432aaca8981ce` |
| Original launcher | `0b89a344ff84dbc126e88b88bb714c6b17429843` |
| Recovery shared UI | `817c2985d371f8d19d70d9d1d9629425ed6cfd68` |
| New developer handoff | `7db77bb`, replayed with attribution as `9ff8953` |

The game and engine remotes are preserved, as are separate original game and launcher worktrees for comparison. The engine used by the recovery game is an unchanged upstream commit. Reconstructing the intended topic changes showed that replaying the entire fork would retain a large superseded architecture. The selected baseline therefore reuses upstream implementations and ports game-owned capabilities directly.

## Dispositions

| Change group | Decision and evidence |
|---|---|
| DuckStation source retirement / invalidation, `008d3d4a`, `7bc8663e` | Already upstream in behavior; both replay as empty patches. Skip duplicates. |
| Fork guest hot-path / timing / inline-memory package | Use current upstream native spans and its execution implementations instead. Regenerated from the verified disc; optimization switches remain enabled. |
| Quality presets and unsupported video keys | Migrate to current `[video]` schema; remove obsolete quality tables, `dynamic_resolution_start_min` and `present_thread_slots`. Strict key check passes. |
| Fork launcher | Preserve original; use current shared UI for compatible current engine APIs. Custom R4 screens are not yet ported. |
| Adaptive widescreen, PGXP, HD HUD, detail, camera, controls | Retain game-owned implementations. Unit/site checks and PGXP boot pass; moving visual checks determine preview acceptance. |
| Car effects and HUD sampling | Retain guest-clock fixes and port off packet metadata. Fix the inherited unprotected payload copy; a contention/publication regression test passes. Effects alignment remains part of visual review. |
| Rewind default integration | Replace the fork-only `video.rewind` behavior with a game-targeted override of upstream's supported `psx.feature.rewind` package. Verified enabled with 50 snapshots, guest frozen while open and resumed after cancel. |
| Fork packet interpolation and draw-isolation tooling | Omit from this baseline, including their package controls. The developer's issue #106 also identifies the 60 FPS path as the remaining artifact source. Do not treat the original 30 Hz race cadence as recovered 60 FPS presentation. |
| Fork native-authority/pooled-surface/hold-to-rewind architecture | Do not replay wholesale. Current upstream owns rendering and filmstrip rewind; the hold-to-rewind experience remains an unrecovered enhancement. |
| Live Graphics WIP mentioned in new handoff | Preserve as a reference; do not integrate unverified WIP during baseline recovery. |

## Validation evidence

- Disc BIN SHA-1: `75F0DAB5C1F71BA80F3D1E2FEFD043F3F2B9EE2E`; CUE SHA-1: `7FB365C31E0A1F315479AC9EEF1D6FDE1A84BD1D`, matching DISC.md. The supplied archive remains untouched; extracted working disc is local in `disc/`.
- Supported upstream generation succeeds, producing 52 ordinary game shards. Generated C is never hand-edited. The emitter itself produces trailing whitespace; this is not cleaned by rewriting its output.
- Windows Release diagnostic build passes with PGXP and native CPU spans enabled. All 26 CTests pass, including actual PGXP boots, mod-plugin audit, widescreen/detail disc-site checks and the sampler contention test.
- Existing 40-step hidden smoke reaches race frame 8547 with zero dispatch or segment misses. Current-build save/load slot 8 succeeds. Host pause/resume and 1280x720, 1440x600 and 960x720 window resize requests succeed. A race capture was visually inspected, but this alone does not establish moving culling/seam correctness.
- A first autopilot attempt remained at the grid because the guest-button script and Modern trigger mapping did not agree. Reject it as moving coverage. Subsequent automated comparisons explicitly select Classic only in their disposable profiles; human/player settings are not changed.
- Further moving screenshots and original/recovery measurements are recorded in `analysis/recovery/compare/`. Final observations are appended below after inspection.
- The supplied live Modern Controls test passes all eight applicable checks after preparing a checkpoint beyond the starting countdown: NeGcon identification, RT/LT sweeps, steering, R1 camera, Y rewind/cancel, pause-menu stock input and zero dispatch/segment misses. No physical-controller or manual-transmission claim follows from this virtual host-input test. The initial failed report is retained; its camera taps occurred during the countdown, its rewind package was disabled, and its maximum-steer yaw sample differed by one unit. Neither the strict test nor its thresholds was weakened.

### Visual inspection completed so far

The owner requested screenshots specifically for culling, draw distance and seams. Inspected composed-output screenshots from a moving 45-second route and a further 105-second route on Helter Skelter, including the starting straight, underpasses, construction/bridge scenery, tight tunnel bends and tunnel exits. Changed to the third-person camera to inspect the player model and its relation to the road. Later route samples approach the start section again; this is representative course coverage, not an all-track audit or an exit/finish test.

Road and side walls remain continuous in the inspected captures, tunnel roofs stay closed, trackside trees and bridge supports remain visible, and extended distant buildings/structures are present. No large sky-coloured cracks or missing road panels were seen in these samples. The first resize check also includes real extra scenery in the wide margins and HUD anchors within the output.

These are sampled images, so they cannot exclude single-frame pops between captures. The far-left background in detail-100 contains thin horizontal distant features; their fidelity is unclassified, not silently counted as a pass. Car Effects' exhaust/smoke/sparks are retained but their individual triggers have not been qualified. An HD-texture live on/off diagnostic is recorded separately: its first held-frame capture cannot demonstrate a changed texture, and subsequent images use native HUD textures after tracking reset. Do not count that later sequence as a full HD-HUD appearance test.

Useful local images under `analysis/recovery/compare/recovery/` are `visual-24.png`, `detail-060.png`, `detail-080.png` and `detail-100.png`. Full PGXP remains enabled throughout; the route finishes with zero dispatch/segment misses. The original comparison build is retained for comparative checks.

### Performance comparison

Two 20-second moving arms per build on the same Windows PC, at the 1080p preset/effective 5x, supersample 1, 1280x720 window, full PGXP and original race cadence:

| Build | Guest VBlanks/s | Process CPU seconds per arm |
|---|---|---|
| Original fork | 54.641, 52.534 | 30.078, 29.531 |
| Recovery | 59.982, 58.592 | 27.266, 25.750 |

This is an improvement in the tested package, with less process CPU time. It is not a locked-60 qualification: the second recovery arm still reports audio starvation. Original and recovery use different rewind implementations, so do not attribute the whole difference solely to native CPU spans. Checkpoints are independently prepared in the same race, not interchangeable save states. The two short ordered arms do not establish results at other resolutions, on other tracks or under other host loads. Ring-based aggregate frame-time percentiles include earlier activity and are not used as 20-second tail measurements.

The developer's own `tools/perf/measure.py` also ran against the original fork at requested 60 FPS from a moving checkpoint: 254 of 550 packets replaced (46%), 12.1 ms prepare per consumed packet, 23.3 ms per draw and only 145 swap completions in its roughly ten-second sampled interval. This is a separate diagnostic configuration, not an equal-package comparison with the recovery. It supports withholding that interpolation path rather than importing it wholesale.

Exact binary identities, per-arm numbers and limits are in [validation.json](validation.json); raw results, process ownership/capacity receipts and screenshots remain in the local `analysis/recovery/` directory.

### Owner preview prepared

The recovery preview is open in a normal 1280x720 window, paused in the game's own Pause menu in third-person view. Press Start to resume; Modern controls use RT for acceleration and R1 for the camera. Normal audio is enabled, with no dummy-driver override. The private profile uses 1080p/1x supersampling and Fit to screen, and has its own settings and saves under `analysis/recovery/playtest/`. Its setup screenshot is `ready.png`; observations can be saved in `owner-notes.txt`. Read those notes before any follow-up work, and do not interrupt a live owner playtest.

The original game and UI are preserved in the `RidgeRacerType4Recomp-original` worktree. The standalone engine clone also has the parked performance/review branches; switch it to `archive/r4-original-20261102` before rebuilding the original comparison tree, which was configured against that clone. The game recovery has its own independent upstream submodule checkout.

Do not infer physical scanout cadence, complete track coverage, netplay qualification, macOS/Linux qualification, release PGO gains or 8K/120 Hz capability from these checks. No shared engine patch was introduced, so a new consumer-title engine regression campaign is not required for this baseline.

## Reproduction and retained data

Configure with Release, `R4_BUILD_TESTS=ON`, `PSX_DEBUG_TOOLS=ON` and the current supported compiler. Regenerate through `tools/regen.sh` / `psxrecomp_cli.py generate`; do not turn off native spans to bypass stale generated sources. Run CTest and `tools/smoke.py` against an isolated hidden/muted runtime. `tools/perf/autopilot.py` from the developer's handoff supplies the moving route. Its packet-specific `measure.py` is appropriate to the original fork; current upstream's telemetry has a different contract.

Local evidence and bounded comparison scripts are in ignored `analysis/recovery/`; built output is in `build/`. Original comparison output is outside delivery folders. Retain the verified disc, candidate binary, useful captures, numerical results and profiles for owner review. Remove disposable extraction/build/test data after the review when it is no longer needed. Native saves and explicit player settings are never replaced.

Read [the technical assessment](TECHNICAL_ASSESSMENT.md) and [the collaborator-facing assessment](COLLABORATOR_ASSESSMENT.md) for findings and prevention measures. The latter is a draft for the owner; it has not been sent to anyone.

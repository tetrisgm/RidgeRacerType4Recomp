# Upstream recovery and owner preview

This is a local development recovery on `recovery/upstream-20261102`. It is not a release or a claim that every fork enhancement has been recovered. Nothing has been pushed or merged remotely.

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

Do not infer physical scanout cadence, complete track coverage, netplay qualification, macOS/Linux qualification, release PGO gains or 8K/120 Hz capability from these checks. No shared engine patch was introduced, so a new consumer-title engine regression campaign is not required for this baseline.

## Reproduction and retained data

Configure with Release, `R4_BUILD_TESTS=ON`, `PSX_DEBUG_TOOLS=ON` and the current supported compiler. Regenerate through `tools/regen.sh` / `psxrecomp_cli.py generate`; do not turn off native spans to bypass stale generated sources. Run CTest and `tools/smoke.py` against an isolated hidden/muted runtime. `tools/perf/autopilot.py` from the developer's handoff supplies the moving route. Its packet-specific `measure.py` is appropriate to the original fork; current upstream's telemetry has a different contract.

Local evidence and bounded comparison scripts are in ignored `analysis/recovery/`; built output is in `build/`. Original comparison output is outside delivery folders. Retain the verified disc, candidate binary, useful captures, numerical results and profiles for owner review. Remove disposable extraction/build/test data after the review when it is no longer needed. Native saves and explicit player settings are never replaced.

Read [the technical assessment](TECHNICAL_ASSESSMENT.md) and [the collaborator-facing assessment](COLLABORATOR_ASSESSMENT.md) for findings and prevention measures. The latter is a draft for the owner; it has not been sent to anyone.

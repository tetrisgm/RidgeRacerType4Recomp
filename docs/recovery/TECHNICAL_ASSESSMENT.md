# R4 fork assessment, 2026-11-02

This assessment distinguishes inspected code/history, the project's own historical measurements, and new local validation. Historical numbers below have not been independently reproduced unless the recovery checklist explicitly says so.

## What was forked

The game is a standalone repository, not a GitHub fork. Its engine submodule points at tetrisgm/psxrecomp, a fork of RetroPortingToolKit/psxrecomp. The game's 14d798191852 head pins engine 84d6ca22259ad16d9dd5e13f390514e575cb8d10, the fork's r4/main line. The fork's master is an older upstream ancestor and is not the relevant development head.

Frozen upstream is c1f5cafd4102daa57b7342bec7c432aaca8981ce. Its common ancestor with the pinned fork is d19d8c7ef468577ede65b0b9cc4ee8fbce17717d. The fork has 321 commits absent from upstream ancestry and lacks 212 upstream commits. These are ancestry counts, not counts of unique features: merges, backports and equivalent implementations make simple commit counts misleading.

The fork changes 321 files within runtime/ and recompiler/ relative to the common ancestor, adding 33,106 lines and deleting 6,282. Generated output and other directories are excluded from that figure. The renderer alone adds 4,480 and removes 807 lines. This is a substantial engine development line, not a small game patch.

## What worked and deserves credit

The goals are useful: adaptive widescreen, cleaner geometry, HD HUD, better detail, modern controls, reduced renderer overhead and smoother presentation. The project retained regression fixtures, pin history, raw numerical summaries and unusually candid failed-gate reports. Several small optimizations are worth retaining.

For example, replaying 008d3d4a (linear DuckStation texture-source retirement) and 7bc8663e (skip invalidation work when no tracked upload intersects) onto current upstream produced empty cherry-picks. Their behavior is already represented upstream, including e7c3c57c. Patch identity alone had marked them fork-only; actual replay resolved the decision. Do not carry duplicates or discard their contribution.

## Where the implementation went off course

1. **Upstream compatibility became a long-lived blocker.** docs/UPSTREAM.md records the full sync stopping at d108e9a0 on October 24 because upstream #766 removed Graphics presets and fields R4 still used. Selective backports followed. The real migration requirement was left unresolved while more engine changes accumulated. Current upstream includes native spans, ordered batching, host work improvements and selective release PGO. Initial local configure rejects the old generated game C; initial regeneration rejects [quality.*]. This confirms concrete integration work was missing. It does not yet quantify a runtime loss.

2. **A broad replacement was integrated before all equivalence targets were established.** Native authoritative VRAM, asynchronous packets, pooled presentation surfaces, geometry interpolation, audio transport, rewind and guest execution all moved. FRAME_INTERPOLATION_STATUS.md records failures and explicitly distinguishes frozen packet endpoint checks from an independently booted stock renderer. Later HANDOFF.md records a successful 120-second cadence/underrun run but also says it does not establish physical 8K scanout or every visual-equivalence target. A successful narrower gate cannot close those other targets by implication. This is an evidence gap, not proof that every phase remained broken.

3. **Latest-wins packets were not initially self-sufficient for enhanced rendering.** Dropping a packet could invalidate enhanced surfaces and reseed a buffer from native VRAM, causing single-frame 4:3/low-resolution flashes. Interpolation also bridged packets separated by lost frames. 66e06a96 and 734794d5 address these problems. The general lesson is to prove that skipped producer work cannot erase state needed by the next consumer output, including double buffers, texture inputs and epochs.

4. **Geometry matching confused identity with enumeration.** 8d14308b replaces draw-order instance ordinals that changed when trees entered or left the cull set. The old approach could match a vertex to another tree and put it in the sky. The replacement uses nearest translations with a threshold; that is a heuristic needing adversarial tests, not independent proof of persistent identity. A zero-unmatched counter can still describe a wrong match.

5. **Surface ownership changed without updating all consumers.** 91b54cd2 repairs bloom's assumption that s_hr_fbo always represented full scaled VRAM. A regional surface mapped the HUD mask into about 37% of the output. This illustrates why renderer redesigns require reviewing post-processing, diagnostics, pause, restore and presentation together, beyond the primary draw path.

6. **Defaults and documentation outran a stable feature contract.** README and historical notes disagree about optional frame-rate defaults and dynamic resolution. R4's config retains quality tables that current upstream rejects and two video keys it does not read. Some tests assert implementation strings or old defaults rather than independent output. Local recovery removes obsolete config and unsupported package controls instead of pretending they work.

## Recovery decisions

Use current upstream unchanged as the engine baseline; do not mechanically merge the entire fork. Retain game-owned controls, widescreen, PGXP, HD textures, detail, HUD and effects as candidates to validate. Remove fork packet interpolation and its draw-isolation tool from this recovery branch; preserve their complete sources in archive/original-20261102 and the engine archive branch. Do not replace them with image warping under the same feature name.

The game C is regenerated from the hash-verified disc using current upstream, with native CPU spans enabled. The packet metadata dependency is removed from the race sampler while its pause/speedometer observations and guest-clock effects fixes are retained. That sampler still needs concurrency and presentation-alignment qualification before effects are called recovered. The fork's hold-to-rewind UX is not equivalent to upstream's filmstrip rewind and must be recorded as an unrecovered enhancement.

No benchmark win, complete visual recovery, cross-game qualification or release readiness follows from source compatibility or a passing build. Consult README.md in this directory for actual validation status.

## Developer handoff received during recovery

The owner supplied the developer's handoff screenshot. Commit 7db77bb adds docs/DEBUGGING.md and tools/perf; it is replayed unchanged with attribution as 9ff8953. Issue #106 describes the remaining packet-gap artifacts and reports that selecting original race cadence removes the prepare cost and artifacts. This supports establishing the upstream/native-cadence control before further interpolation work. Its PC/Mac costs remain the developer's measurements, not independently reproduced numbers.

The supplied autopilot is reused. Its first run with a Modern input transform stayed at the grid, so those images are rejected as moving coverage. Classic is selected only in isolated automation profiles; the next run visibly moves through the course. This is a useful example of validating the workload itself before accepting an ostensibly successful performance or graphics run.

The guide's statement that all visible symptoms come from packet gaps is broader than the evidence establishes: it also lists unbisected seams and a separate guest-side spike. Keep those as separate hypotheses until tested. The live Graphics branches mentioned there remain unmerged and unverified, so they are preserved as references rather than added to this recovery baseline.

## Source references

- Engine comparison: https://github.com/RetroPortingToolKit/psxrecomp/compare/c1f5cafd4102...84d6ca22259ad16d9dd5e13f390514e575cb8d10
- Original game records: docs/UPSTREAM.md, docs/framework_pin_history.md, docs/FRAME_ARCHITECTURE.md, docs/FRAME_INTERPOLATION_STATUS.md and docs/HANDOFF.md at archive/original-20261102.
- Original engine repairs: 734794d5, 8d14308b, 91b54cd2 and a44c5467, preserved in the local engine archive.

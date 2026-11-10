# Upstream fixes

R4 pins fork master `9fbb6ba8` (upstream `c1f5cafd` plus the changes below).
The fork has one R4 development line, master. PR heads are isolated review
artifacts; the frozen performance baseline is not replaced by old topic heads.

| Fix | Upstream PR | Original commit |
|---|---|---|
| HD upload residency across rewind | [#860](https://github.com/RetroPortingToolKit/psxrecomp/pull/860) | `b9dcf919` |
| HD replacement coverage over transparent native texels | [#861](https://github.com/RetroPortingToolKit/psxrecomp/pull/861) | `3a0502c5` |
| XA music alone is not a rewind movie | [#862](https://github.com/RetroPortingToolKit/psxrecomp/pull/862) | `006fb5bb` |
| Direct-launch Steam Controller policies and HIDAPI | [#866](https://github.com/RetroPortingToolKit/psxrecomp/pull/866) | `fb34f318` |
| Complete menu settings, persistent volume and state host wiring | [#867](https://github.com/RetroPortingToolKit/psxrecomp/pull/867) | `ae3eae8f`, `b18d4864`, `e3aabde0` |
| Drain rendering before debug window resizing | [#869](https://github.com/RetroPortingToolKit/psxrecomp/pull/869) | `d585a728` |
| Batched render queue publication | [#874](https://github.com/RetroPortingToolKit/psxrecomp/pull/874) | `e57db5da` → `faf7bcb3` |
| GL blend/stencil capability cache | [#875](https://github.com/RetroPortingToolKit/psxrecomp/pull/875) | `63677cb7` → `7330aa66` |
| Fixed shader sampler units | [#876](https://github.com/RetroPortingToolKit/psxrecomp/pull/876) | `cc413ceb` → `b1bfaa79` |
| Allocation-free HD misses | [#877](https://github.com/RetroPortingToolKit/psxrecomp/pull/877) | `b7f91c6e` → `7531a8a7` |
| Native snapshot copy on render owner | [#878](https://github.com/RetroPortingToolKit/psxrecomp/pull/878) | `a1af76d0` → `7b93a5c8` |
| Opt-in continuous threaded debug readback | [#879](https://github.com/RetroPortingToolKit/psxrecomp/pull/879) | `c7b69a3d` |
| Scissored hardware PGXP depth clears | [#880](https://github.com/RetroPortingToolKit/psxrecomp/pull/880) | `7f71774f`, test registration `9fbb6ba8` |

Each PR isolates one fix over `c1f5cafd`. #867 now includes the complete custom
menu host adapter; its review version omits #866’s independent adopted-pad
policy, while the qualified integration contains both. Review heads are not
separately runtime-qualified. Current Mac integration validation includes
Release, 29 CTests, 40-step hidden/dummy-audio smoke
with zero dispatch/segment misses, plus real in-game settings/pause/navigation
checks, actual save/load and settings persistence across restart. See
[restoration validation](archive/CUSTOM_MENU_20261109.md). Controller policy
tests and synthetic SDL events do not qualify a
physical Steam Controller or Windows. Earlier recovery evidence includes the
actual-ring rewind test and 19 real-GL HD fixture checks. PRs state their
integration scope explicitly; a newer upstream pin requires fresh qualification.

The four performance candidates are qualified together; no per-fix speedup is
claimed. [The restoration receipt](recovery/MSTAN_RESTORATION_20261109.md) records
the exact GL parity checks, remaining candidates and limited 4K improvement.

Once these fixes merge, test the new upstream pin, regenerate as required,
record it in `framework_pin_history.md`, switch `.gitmodules` upstream and
archive the fork. Retire the PR branches after landing. Do not merge
unrelated upstream changes merely to remove the fork early.

Recovered research leads now have durable game issues:
[#108 idle-wait](https://github.com/tetrisgm/RidgeRacerType4Recomp/issues/108)
and [#109 renderer/rewind/audit experiments](https://github.com/tetrisgm/RidgeRacerType4Recomp/issues/109).
See [branch dispositions](archive/BRANCH_DISPOSITION_20261108.md).
The [old ledger](archive/UPSTREAM-before-consolidation-20261108.md) is historical.

The November 10 fixes reach full native game cadence in the qualified 120-second
fixed-4K Mac race. See [the performance fix](recovery/PERFORMANCE_FIX_20261110.md)
for exact depth-attachment parity, snapshot correctness and measurement limits.

# Upstream fixes

R4 pins fork master `ae3eae8f` (upstream `c1f5cafd` plus the changes below).
The fork has one R4 development line, master. PR heads are isolated review
artifacts; the frozen performance baseline is not replaced by old topic heads.

| Fix | Upstream PR | Original commit |
|---|---|---|
| HD upload residency across rewind | [#860](https://github.com/RetroPortingToolKit/psxrecomp/pull/860) | `b9dcf919` |
| HD replacement coverage over transparent native texels | [#861](https://github.com/RetroPortingToolKit/psxrecomp/pull/861) | `3a0502c5` |
| XA music alone is not a rewind movie | [#862](https://github.com/RetroPortingToolKit/psxrecomp/pull/862) | `006fb5bb` |
| Direct-launch Steam Controller policies and HIDAPI | [#866](https://github.com/RetroPortingToolKit/psxrecomp/pull/866) | `fb34f318` |
| In-game session volume host wiring | [#867](https://github.com/RetroPortingToolKit/psxrecomp/pull/867) | `ae3eae8f` |

Each PR isolates one fix over `c1f5cafd`. Current Mac integration validation
uses all five together: Release, 29 CTests, 40-step hidden/dummy-audio smoke
with zero dispatch/segment misses, plus real in-game settings/pause/navigation
checks. Controller policy tests and synthetic SDL events do not qualify a
physical Steam Controller or Windows. Earlier recovery evidence includes the
actual-ring rewind test and 19 real-GL HD fixture checks. PRs state their
integration scope explicitly; a newer upstream pin requires fresh qualification.

Once these fixes merge, test the new upstream pin, regenerate as required,
record it in `framework_pin_history.md`, switch `.gitmodules` upstream and
archive the fork. Retire the PR branches after landing. Do not merge
unrelated upstream changes merely to remove the fork early.

Recovered research leads now have durable game issues:
[#108 idle-wait](https://github.com/tetrisgm/RidgeRacerType4Recomp/issues/108)
and [#109 renderer/rewind/audit experiments](https://github.com/tetrisgm/RidgeRacerType4Recomp/issues/109).
See [branch dispositions](archive/BRANCH_DISPOSITION_20261108.md).
The [old ledger](archive/UPSTREAM-before-consolidation-20261108.md) is historical.

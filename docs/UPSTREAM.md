# Upstream fixes

R4 pins fork master `006fb5bb` (upstream `c1f5cafd` plus the three changes below).
The fork master was fast-forwarded without rewriting history on 2026-11-08.
Its old recovery/integration/topic refs are pending retirement; they are not
additional development lines.

| Fix | Upstream PR | Original commit |
|---|---|---|
| HD upload residency across rewind | [#860](https://github.com/RetroPortingToolKit/psxrecomp/pull/860) | `b9dcf919` |
| HD replacement coverage over transparent native texels | [#861](https://github.com/RetroPortingToolKit/psxrecomp/pull/861) | `3a0502c5` |
| XA music alone is not a rewind movie | [#862](https://github.com/RetroPortingToolKit/psxrecomp/pull/862) | `006fb5bb` |

Each PR contains one fix over `c1f5cafd`. Mac integration validation uses all
three together: Release, CTest 26/26, 40-step hidden/dummy-audio smoke with zero
dispatch/segment misses; actual-ring rewind test passes and real-GL HD fixture
passes 19 checks. PRs state this scope explicitly. Current upstream `9b914263`
was inspected but not adopted without regeneration and full game validation.

Once all three merge, test the new upstream pin, regenerate as required,
record it in `framework_pin_history.md`, switch `.gitmodules` upstream and
archive the fork. Retire the three PR branches after landing. Do not merge
unrelated upstream changes merely to remove the fork early.

Recovered research leads now have durable game issues:
[#108 idle-wait](https://github.com/tetrisgm/RidgeRacerType4Recomp/issues/108)
and [#109 renderer/rewind/audit experiments](https://github.com/tetrisgm/RidgeRacerType4Recomp/issues/109).
See [branch dispositions](archive/BRANCH_DISPOSITION_20261108.md).
The [old ledger](archive/UPSTREAM-before-consolidation-20261108.md) is historical.

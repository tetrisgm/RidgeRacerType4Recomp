# Handoff

## Current — 2026-11-09

Game development is on `master`, with one local/remote game branch and one
registered worktree. Engine fork master is `ae3eae8f`; UI remains upstream
`817c2985`. All changes are intended for the consolidated finalization baseline.

[Consolidation decisions and recovery](CONSOLIDATION.md) explain the retained
MStan/upstream performance work, replaced inactive menu integration, recovered
Steam Controller fixes and deferred renderer experiments. The 62 additional
game topics and 44 old worktrees are preserved locally and retired from active
work. The earlier 36 approved deletions remain in the historical receipts.

R4 now has a shared-UI in-game menu over the running game. Keyboard, controller
menu events, mouse resume, pause/resume, mod persistence/restart notices and
PGXP's locked enable switch have been checked. Full rebinding, disc/card and
online setup remain in the full launcher, which ends the current session.

## Verification and package

Mac Release and 29 CTests pass. Hidden/dummy-audio smoke with
`present_thread = false` records 7,203,494 native overlay dispatches, no R4
interpreted overlay PCs and zero dispatch/segment misses. Evidence:
`analysis/consolidation-20261109/consolidated-final/`.

The portable package is `dist/windows/2026-11-09-consolidated-01/`, built using [WINDOWS_BUILD.md](WINDOWS_BUILD.md),
with 7,459 linked variants, system/UCRT-only imports and checksums. The NAS is
unrelated to development and delivery. Never connect to or operate STEAMBOX.
The owner chooses how to transfer the portable folder and tests it by hand.

## Remaining finalization

- Owner Windows race/menu/physical Steam Controller playtest, including the
  normal-paced rank-8 route and 4K/Max Detail slowdown. Synthetic controller
  events and turbo smoke do not prove those outcomes.
- Review [upstream fixes](UPSTREAM.md), then qualify and record an upstream pin
  before archiving the engine fork. PR branches are review artifacts.
- Exhaustive game-mode/indirect-target coverage is not established by smoke.
  Research leads remain game issues #106, #108 and #109.

# Handoff

## Current — 2026-11-09

Engine `b57e486e` restores four performance candidates from MStan's preserved
ledger: render-ring batching, GL capability caching, fixed sampler units and
allocation-free HD misses. Custom UI `ef8150f`, Select/Back, controls/settings,
save/load and Steam Controller fixes remain intact. PGXP stays required.

[Restoration and candidate audit](recovery/MSTAN_RESTORATION_20261109.md)
records the exact dispositions, tests and performance limits. Local evidence:
`analysis/mstan-restoration-20261109/`. Mac Release, 29 CTests, ring/GL/HD
fixtures and actual menu/persistence/native-overlay smoke pass. New portable
candidate: `dist/windows/2026-11-09-mstan-restored-01/`; use its BUILD.json,
execution-contract sidecar and SHA256SUMS. Earlier packages omit these four
optimizations.

## Remaining qualification

Paired 4K grid tests improve from 45.896 to 47.306 VBlanks/s (about 3.1%) on
the M4 under background load. This remains below full speed. The prior 1440p
qualification is recorded in [Mac measurements](archive/MAC_PERFORMANCE_20261109.md);
owner settings remain unchanged. Do not claim the restoration fixes all 4K
slowdowns. Disc prefetch, Windows priority and larger architecture candidates
have explicit dispositions in the new audit.

Visible-window pacing/audio, the owner's specific rank-8 crash route, physical
Steam Controller/Steam Input, effects/camera and all-course/multiplayer play
remain to qualify. Windows execution is owner-only. NAS and STEAMBOX are not
development machines for this work.

## References

- [History audit](recovery/HISTORY_AUDIT_20261109.md)
- [Menu restoration](archive/CUSTOM_MENU_20261109.md)
- [Upstream reviews](UPSTREAM.md) / [pin history](framework_pin_history.md)
- [Windows packaging](WINDOWS_BUILD.md)

Game development has one master branch and one checkout. Historical sources
remain at `/Users/shokunin/dev-archives/r4-20261109/`.

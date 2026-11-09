# Handoff

## Current — 2026-11-09

The owner requires the original custom R4 menu integrated over the game,
including controls and settings, accessible with Select/Back alone. Keep
MStan’s recovery/performance work and the Steam Controller fixes. The generic
menu in committed `f9643421` does not satisfy this requirement.

The [history audit](recovery/HISTORY_AUDIT_20261109.md) records the chronological
and source analysis, keep/restore/rework decisions, measurement limits and
execution order. Its [scope receipt](recovery/HISTORY_AUDIT_20261109.json)
identifies all indexed history, compared topics and unavailable old pins.
The original [consolidation record](CONSOLIDATION.md) is historical.

## Provisional restoration — not qualified

Uncommitted work currently selects custom UI `0b89a344`, restores
`launcher/title_r4.cpp` and its CMake integration, adds three launcher-header
compatibility fields, and ports the old menu save-state host API. The initial
custom-menu build passed before the save-state host port; no complete custom
menu runtime qualification has been performed. The build directory is not the
committed baseline binary. Patches are saved in
`analysis/history-audit-20261109/provisional-*`.

Do not call compilation restoration complete. Audit findings require adapting
controls/device settings, host-owned graphics schemas, state operations,
mounted-disc/artwork information, and lifecycle/input behavior. Follow the
restoration contract in the audit before choosing further changes.

## Preserved baseline and remaining work

Game `f9643421` pins engine `ae3eae8f` and upstream UI `817c2985`. Its recorded
Mac Release/29 CTest/native-overlay checks passed; evidence is
`analysis/consolidation-20261109/consolidated-final/`. The portable
`dist/windows/2026-11-09-consolidated-01/` contains the generic menu. Rebuild a
new version after custom-menu qualification; do not relabel that package.

Development has one local/remote game branch and one registered worktree.
Historical sources and 44 old worktree directories are preserved locally at
`/Users/shokunin/dev-archives/r4-20261109/`. Missing historical submodule objects
and raw Windows recovery traces are documented in the audit; do not claim they
were independently verified. NAS is unrelated; never access it for this work.
Never connect to or operate STEAMBOX.

The crowded-race 4K slowdown is unresolved. Normal-paced rank-8 crash regression,
physical Steam Controller/Steam Input, effects alignment, camera comfort,
all-course and multiplayer qualification remain separate from menu acceptance.
[Upstream review](UPSTREAM.md) and [Windows packaging](WINDOWS_BUILD.md) remain
the applicable references. Preserve owner saves, PGXP and explicit settings.

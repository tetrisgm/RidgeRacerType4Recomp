# Handoff

## Current — 2026-11-09

The original custom R4 menu is restored through `launcher/title_r4.cpp` and
custom UI `ef8150f`. Select/Back alone opens it over the game. Controls,
bindings, device/deadzone settings, display, volume and save/load are wired to
the current runtime. Engine `d585a728` preserves the MStan recovery and Steam
Controller baseline. PGXP remains required; Resume keeps restart-only edits
pending without discarding the race.

[Restoration validation](archive/CUSTOM_MENU_20261109.md) records the Mac
Release, 29 CTests, UI fixtures, actual menu/save-load/persistence checks and
native boot-to-race smoke. Local evidence is
`analysis/custom-menu-20261109/qualified-02/`. The portable Windows candidate is
`dist/windows/2026-11-09-custom-menu-01/`, cross-built on the Mac; use its
checksums and BUILD.json. Prior consolidated packages have the generic menu.

## Remaining qualification

The crowded-race 4K slowdown is unresolved. Normal-paced rank-8 regression,
physical Steam Controller/Steam Input, effects alignment, camera comfort,
all-course and multiplayer play still need qualification. Windows execution
is owner-only. Never access STEAMBOX or the unrelated NAS for development.

## References

- [History audit and unavailable evidence](recovery/HISTORY_AUDIT_20261109.md)
- [Upstream review](UPSTREAM.md) and [pin history](framework_pin_history.md)
- [Windows packaging](WINDOWS_BUILD.md)
- [Historical consolidation](CONSOLIDATION.md)

Game development remains on one local/remote master branch and one working
checkout. Historical sources/old worktree directories remain locally preserved
at `/Users/shokunin/dev-archives/r4-20261109/`.

# Handoff

## Current — 2026-11-10

Engine `9fbb6ba8` fixes the measured 4K critical path: native snapshot copies
stay on the render owner, PGXP depth resets use exact scissored hardware clears,
and threaded debug runs no longer continuously read back VRAM by default.
Custom UI `ef8150f`, Select/Back, settings/controls and Steam fixes remain.

[Qualification and measurements](recovery/PERFORMANCE_FIX_20261110.md): M4
moving race sustains 59.936 VBlanks/s for 120 seconds at fixed 4K, full PGXP,
Max Detail and rewind. Real-GL depth/snapshot/HD fixtures, Release, 29 CTests
and native menu/race smoke are the validation gates. New portable candidate:
`dist/windows/2026-11-10-4k-performance-01/`; use its BUILD.json and checksums.

## Remaining qualification

Visible-window pacing/audio, physical Steam Controller/Steam Input, Windows
execution, all courses, the owner's specific rank-8 route and multiplayer remain
to qualify. The two-minute test establishes native game cadence, not 60 unique
images/s or physical display scanout. Full prior threading evaluation remains
in [the audit](recovery/THREADING_EVALUATION_20261109.md); do not reintroduce the
old packet architecture to replace this measured fix. NAS and STEAMBOX are not
development machines.

## References

- [History audit](recovery/HISTORY_AUDIT_20261109.md)
- [Menu restoration](archive/CUSTOM_MENU_20261109.md)
- [Upstream reviews](UPSTREAM.md) / [pin history](framework_pin_history.md)
- [Windows packaging](WINDOWS_BUILD.md)

Game development has one master branch and one checkout. Historical sources
remain at `/Users/shokunin/dev-archives/r4-20261109/`.

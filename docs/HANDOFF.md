# Handoff

## Current work — 0.2.2 release verification

Engine `d81b3919` fixes rewind release by restoring already-issued rendering
work and using safe checkpoint anchors. It also separates Mac worker drawables.
Menu/controller/UI code remains unchanged. All 44 tests pass; three exact
restores and 130 full-resolution comparisons match. Native Mac two-minute
4K race: 59.86 VBlanks/s, 3,599/3,599 intermediate frames, maximum guest frame
35.542 ms. Five internal releases: 27–47 ms; observed input-to-image: 31–65 ms.

Packaging 0.2.2 is in progress. Do not label it finished until signed Mac bundle
checks/notarization and native Windows package checks are recorded. Prior 0.2.1
packages remain intact as historical baselines.

Tests remain muted. Owner will separately check audible audio and physical
Steam Controller gameplay. No NAS, public release/feed or unrelated process
changes. Owner authorized SSH PC tests and an exact temporary EXE ASR exception;
read the PC runbook, use the previously approved path, and restore protections.

## References

- [Current Mac receipt](recovery/MAC_RELEASE_20261112.json)
- [Earlier investigation](recovery/REWIND_RELEASE_20261112.md)
- [Rewind implementation](REWIND_PLAYBACK.md)
- [Pin history](framework_pin_history.md)
- [Mac workflow](MAC_BUILD.md) / [Windows workflow](WINDOWS_BUILD.md)

Current grid: `analysis/rewind-release-grid-20261112/memcards`.
Never bypass execution identity. Existing Keychain profile `pp-notary` works.

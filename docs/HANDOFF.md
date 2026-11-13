# Handoff

## Release 0.2.2 — complete locally

- Mac: `dist/macos/R4-0.2.2-macOS-arm64.zip`, Developer-ID signed,
  notarized, stapled and Gatekeeper accepted.
- PC: `dist/windows/R4-0.2.2-Windows-x64.zip`, unsigned portable x64,
  tested natively on the RTX 4080 SUPER at a 3840×2160 drawable.

Engine `d81b3919`, game integration `63f58850`, UI `a786d20`. Rewind restores
already-issued rendering work and uses safe checkpoint anchors. Menu, controls,
settings, MStan performance and Steam Controller fixes remain intact.

Mac: 59.86 VBlanks/s over two minutes, 3,599/3,599 intermediate frames,
maximum guest-frame interval 35.542 ms. Internal release measurements 27–55 ms
across the sustained and final version-stamped checks. PC: 59.96 VBlanks/s over
two minutes, 3,598/3,598 intermediates, maximum sampled presentation interval
19.143 ms, internal releases 42–72 ms. Neither is a zero-latency/all-course promise.

All 44 Mac tests and actual signed-bundle menu/binding/settings restart checks
pass. Exact state/image checks pass on both platforms: Mac 130 comparisons plus
96 in the signed package; PC 108 comparisons. No RAM differences or pixel
mismatches. Reviewed tunnel, sky, ground and consecutive resume captures show no
observed seams, blinking artifacts, displaced HUD or resolution drop. Mac app
was also checked on screen. Zero dispatch misses in tested routes.

Tests were muted. Owner plans separate listening and physical Steam Controller
playtests. No public release/feed was published; 0.2.1 artifacts remain intact.
NAS and unrelated work were untouched. Native PC test task/folder and the exact
EXE ASR exception were removed; original security settings restored, volume
unchanged. Cleanup: `analysis/windows-022-20261112/cleanup.json`.

## References

- [Fix and evidence](recovery/REWIND_RELEASE_FIX_20261112.md)
- [Mac receipt](recovery/MAC_RELEASE_20261112.json)
- [Windows receipt](recovery/WINDOWS_RELEASE_20261112.json)
- [Release notes](../RELEASE_NOTES.md)
- [Rewind](REWIND_PLAYBACK.md) / [pins](framework_pin_history.md)
- [Mac workflow](MAC_BUILD.md) / [Windows workflow](WINDOWS_BUILD.md)

Current grid: `analysis/rewind-release-grid-20261112/memcards`, execution identity
`f3a59eb52c450e6d9f22c0cd78332c02932ca5ef11cbc3c5ffeb9787b8bb9135`.
Never bypass execution identity. Keychain profile `pp-notary` works. The native
PC suites saved passing results before debug `quit` reported busy; the one-off
task cleanup stopped the process. This did not affect recorded race/rewind data.

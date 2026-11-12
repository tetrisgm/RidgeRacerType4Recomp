# Handoff

## Release packages — 2026-11-12

Both local 0.2.1 packages are qualified and prepared:
- `dist/macos/0.2.1/` / `dist/macos/R4-0.2.1-macOS-arm64.zip`
- `dist/windows/0.2.1/` / `dist/windows/R4-0.2.1-Windows-x64.zip`

Mac: Developer-ID signed, notarized, stapled and Gatekeeper accepted. Windows:
unsigned portable x64. Both include checksums, licenses and qualification
receipts; no disc, retail BIOS, developer tools, native saves or owner settings.
No public release/feed was published. Earlier candidates are superseded.

Menu opens with Select/Back; controls/settings, MStan performance and Steam
Controller fixes remain. Hold Y/Triangle/F8 rewinds at gameplay resolution;
release restores the selected state. Runtime engine 20e0e94d/UI a786d20;
source engine a6f0b4b5 adds only a hidden-test VSync fix. All 44 Mac tests and
UI core tests pass. No runtime changes since the verified packages.

Mac visible run: 59.98 VBlanks/s for two minutes, all 3,605 intermediate frames,
max guest frame 46.576 ms, release 99–129 ms. Owner accepted the visual result.
Native PC: RTX 4080 SUPER, 3840×2160 drawable / 2160 internal lines, 59.96
VBlanks/s for two minutes, max sampled present interval 18.904 ms, zero dispatch
misses. Select/Back menu passes. Two native PC rewind restores have zero RAM
differences; all 52 full-resolution image comparisons match. Consecutive sky,
ground and tunnel captures show no observed seams, blink or displaced HUD.
These are measured routes, not all-course or locked 60 guarantees.

Owner requested muted testing and will do hands-on sound/gameplay/controller
checks separately. Do not claim audio listening or physical Steam Controller
QA passed. Source and release notes state this scope.

## PC authorization and cleanup

Owner explicitly authorized SSH PC playtesting on 2026-11-12, overriding the
older owner-only PC testing rule for this task. Read the existing runbook before
future PC work. Defender blocked the unsigned EXE. Owner separately approved
a temporary exception for only that EXE; it was removed after native tests.
`analysis/windows-native-20261112/cleanup.json` confirms original ASR rules and
exclusions restored, task and test directory removed, game stopped, system
volume unchanged. No tools were installed; NAS and unrelated jobs untouched.
Future launches on this hardened PC may again need explicit owner allowlisting.
No ongoing security exception or test job remains.

## References

- [Mac receipt](recovery/MAC_RELEASE_20261111.json)
- [Windows receipt](recovery/WINDOWS_RELEASE_20261111.json)
- [Release notes](../RELEASE_NOTES.md)
- [Mac workflow](MAC_BUILD.md) / [Windows workflow](WINDOWS_BUILD.md)
- [Rewind](REWIND_PLAYBACK.md) / [pins](framework_pin_history.md)
- [Renderer review](recovery/SEPARATE_RENDERER_20261111.md)

Mac motion clip: `analysis/mac-final-motion-20261112/visual-check.mp4`.
Native PC evidence: `analysis/windows-native-20261112/`.
Grid: `analysis/release-pgxp-grid-20261111/memcards`; identity
`769ca791378dc7f82cab1af06580065bf515cbef79182c691f6f6e43c26c07fe`.
Never bypass the guard. Separate scene API has no connected R4 producer.
Apple Keychain pp-notary works; never expose credentials. Isolated CrossOver
bottle R4ReleaseVerification 20261111 remains; native PC evidence is separate.

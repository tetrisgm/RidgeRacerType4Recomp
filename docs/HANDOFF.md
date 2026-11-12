# Handoff

## Current work — 2026-11-12

Finish local Mac and Windows release packages. Mac final is ready at
`dist/macos/0.2.1/` and `dist/macos/R4-0.2.1-macOS-arm64.zip`; signed,
notarized, stapled, Gatekeeper accepted, checksums verified. Windows candidate:
`dist/windows/0.2.1-20261112-rc1/`. No public release/feed or NAS use.

Owner accepted the final Mac visual pass. Visible two-minute run at 3834×2160:
59.98 VBlanks/s, all 3,605 intermediate frames, max guest frame 46.576 ms,
release-to-resume 99–129 ms. No observed tunnel/sky/ground/HUD/seam defects in
reviewed sequences. Mac sound used dummy audio; hands-on sound, gameplay and
physical Steam Controller testing will follow per owner. Do not claim those
were tested. Safe clip: `analysis/mac-final-motion-20261112/visual-check.mp4`.

Select/Back menu, controls/settings, MStan performance and Steam Controller
fixes remain; hold Y/Triangle/F8 rewinds, release restores the selected state.
Packaged runtime engine 20e0e94d/UI a786d20; source engine a6f0b4b5 adds only the
hidden-test VSync fix. All 44 Mac tests and UI core tests pass. Both binaries
pass menu persistence and exact RAM/image rewind checks. No runtime changes
since these qualified packages; regeneration reuses all 7,459 variants.

## PC gate — approval pending

Owner explicitly authorized direct SSH PC testing on 2026-11-12, overriding
CLAUDE.md's older owner-only testing restriction for this task. Read
`~/pc-inventory/STEAMBOX.md`. Existing id_ed25519 works with ramin@192.168.53.188.
PC GPU RTX 4080 SUPER is healthy; desktop 1920×1080. No new dev tools installed.

Temporary folder: `C:\Users\Public\R4ReleaseVerification 20261112`.
One-off interactive task: `R4 Release Verification 20261112` (not automatic).
No game has launched successfully on native Windows: Defender reputation and
ransomware ASR rules blocked the unsigned EXE. No malware detection is listed.
Security policy is unchanged. An async question requests explicit approval
for a temporary **exact-EXE ASR exclusion**, removed after the test. Do not
change security policy before the answer. Saved baseline: asr-before.json in
the PC folder and `analysis/windows-native-20261112/`.

Proposed exception script: `/tmp/r4-pc-exception-plan.ps1` (not executed).
SSH/JSON bridge and driver: `/tmp/r4_pc_control.py`, `/tmp/r4_pc_measure.py`.
Launcher/bridge scripts are in the PC folder. Updated local launch script
removes dummy audio and uses normal display timing; copy before the next test.
Delete only our task/test files and remove any approved exception afterward.
Keep Windows release-qualified false until native verification succeeds.

## References

- [Mac receipt](recovery/MAC_RELEASE_20261111.json)
- [Windows receipt](recovery/WINDOWS_RELEASE_20261111.json)
- [Mac workflow](MAC_BUILD.md) / [Windows workflow](WINDOWS_BUILD.md)
- [Rewind](REWIND_PLAYBACK.md) / [pins](framework_pin_history.md)
- [Renderer review](recovery/SEPARATE_RENDERER_20261111.md)

Grid: `analysis/release-pgxp-grid-20261111/memcards`; identity
`769ca791378dc7f82cab1af06580065bf515cbef79182c691f6f6e43c26c07fe`.
Never bypass its guard. Separate scene API has no connected R4 producer.
No Mac game/test process remains. Other jobs are untouched. Apple Keychain
profile pp-notary works; never expose credentials. Isolated CrossOver bottle
R4ReleaseVerification 20261111 remains; its evidence is not native PC proof.

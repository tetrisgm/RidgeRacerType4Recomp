# Handoff

## Current work — 2026-11-12

Mac and Windows 0.2.1 packages exist locally; **not yet release-qualified**.
Owner requires both finished. No public release/feed, NAS or STEAMBOX operation.
Select/Back menu, controls/settings, MStan performance and Steam Controller
fixes remain. Hold Y/Triangle/F8 rewinds; release restores the selected state.

Packages: `dist/{macos,windows}/0.2.1-20261112-rc1/` and platform ZIPs beside them.
Mac is Developer-ID signed, Apple-notarized, stapled and Gatekeeper-accepted.
Both ZIPs pass archive/checksum checks. Windows is portable x64, unsigned,
system DLLs only, no game data or development tools. Earlier candidates are
superseded; do not distribute them.

Packaged runtime engine `20e0e94d`, UI `a786d20`. Source engine `a6f0b4b5` adds
only the hidden-fixture VSync fix; runtime source is identical. All 44 Mac
CTest cases pass, including three GL fixtures; UI core tests pass. Regeneration
reuses all 7,459 variants. Final signed Mac app and Windows PE both pass isolated
boot/race, menu/save/load and persisted bindings, with zero dispatch misses.
Windows C rename had stranded edited bindings in temporary files; fixed with
filesystem rename. Forward-compatible core GL fixes Mac-hosted WGL creation.

Exact rewind: final Mac binary 21 full-resolution images, Windows packaged PE
13; zero pixel mismatches and zero differing RAM words at resume on both.
Windows checks used CrossOver on the Mac, **not native Windows qualification**.
Final proof is diagnostic, not performance evidence. Earlier normal native
race: 60.05 VBlanks/s, backward 59.55–59.90/s, releases 80/103/161 ms. Signed
RC3 visible 120-second run: 59.86 VBlanks/s, all 3,551 intermediate frames.
No claim of zero latency, sustained locked60, or every-course visual coverage.

## Remaining gates

- Mac is locked. Owner was asked to unlock; final on-screen check pending.
  Native composed menu captures work; CrossOver composed captures were black
  while locked and are not accepted as visual evidence. Prior RC3 live tunnel,
  sky/ground/HUD samples were clean. Its safe clip is
  `analysis/release-rc3-visible-20261111/rewind-visual.mp4`.
- Owner was given Windows ZIP for native 4K race/rewind/menu/Steam Controller
  playtest. Wait for observations. CLAUDE.md forbids operating STEAMBOX.
- Keep release-qualified flags false until remaining gates pass. Do not publish.

Internal rendering 3834×2160; physical display 2560×1440 at75 Hz. Other jobs
remain untouched. No game/test process should be left running. Isolated
CrossOver bottle: `R4ReleaseVerification20261111`; other bottles untouched.
Apple credentials profile `pp-notary` works; never expose credentials.

## References

- [Mac receipt](recovery/MAC_RELEASE_20261111.json)
- [Windows receipt](recovery/WINDOWS_RELEASE_20261111.json)
- [Mac workflow](MAC_BUILD.md) / [Windows workflow](WINDOWS_BUILD.md)
- [Rewind](REWIND_PLAYBACK.md) / [pins](framework_pin_history.md)
- [Renderer investigation](recovery/SEPARATE_RENDERER_20261111.md)
- [Threading review](recovery/THREADING_EVALUATION_20261109.md)

Grid: `analysis/release-pgxp-grid-20261111/memcards`; execution identity
`769ca791378dc7f82cab1af06580065bf515cbef79182c691f6f6e43c26c07fe`.
Never bypass its guard. The separate scene API has no connected R4 producer;
do not describe it as finished. Historical sources: `/Users/shokunin/dev-archives/r4-20261109/`.

# Handoff

## Current work — 2026-11-11

Finish the local Mac release on `master`; follow `CLAUDE.md`. Keep Select/Back
menu, controls/settings, MStan performance and Steam Controller fixes. Rewind
stays enabled: hold Y/Triangle/F8, release at the selected state. No NAS,
STEAMBOX or public release/feed work.

Engine `afb51a4e` / UI `4efe547`: exact command rewind, immutable Mac bundle
storage, keyboard data-path fix, GL ownership retention and bounded first-image
pipeline refill. Release build and 43 CTests pass. Regeneration reused all
7,459 variants without generated-C edits. Build: `build-shipping-20261111`.

Current proof: `analysis/release-warm-proof-20261111/result.json`: three exact
RAM restores, 98 full-resolution image comparisons, zero differences. Normal
30-second run `analysis/release-warm-race-20261111/result.json`: 60.05 VBlanks/s,
59.55–59.90 backward updates/s, complete releases 80/103/161 ms. Improved, not
zero latency or a sustained locked-60 qualification.

Grid: `analysis/release-pgxp-grid-20261111/memcards`; execution identity
`769ca791378dc7f82cab1af06580065bf515cbef79182c691f6f6e43c26c07fe`.
Never bypass the guard. Final signed candidate packaging and visible check are
in progress; then push coherent engine/UI/root commits after required smoke.

RC1 is rejected (keyboard file broke its seal). RC2 is Apple-notarized and
Gatekeeper-accepted, but superseded by the newer rewind fixes; do not mistake
it for the final build. Apple account setup is resolved, profile `pp-notary`.
Never expose credentials. Player data lives in Application Support;
`PSX_BUNDLE_DATA_DIR` isolates tests. Do not change sealed app resources.

Internal rendering is 3834×2160; display is 2560×1440 at75 Hz. Previous live
samples showed clean tunnel/sky/ground/HUD. Physical Steam Controller testing
remains device-dependent. The separate scene API's R4 producer is unconnected;
do not describe it as finished. Do not stop unrelated background jobs.

## References

- [Release receipt](recovery/MAC_RELEASE_20261111.json)
- [Mac package workflow](MAC_BUILD.md)
- [Rewind architecture](REWIND_PLAYBACK.md)
- [Pin history](framework_pin_history.md)
- [Renderer investigation](recovery/SEPARATE_RENDERER_20261111.md)
- [Threading evaluation](recovery/THREADING_EVALUATION_20261109.md)
- [Menu restoration](archive/CUSTOM_MENU_20261109.md) / [upstream](UPSTREAM.md)

Historical sources: `/Users/shokunin/dev-archives/r4-20261109/`.

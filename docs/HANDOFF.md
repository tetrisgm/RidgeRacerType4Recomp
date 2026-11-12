# Handoff

## Current work — 2026-11-11

Preparing the proper Mac release on `master`; follow `CLAUDE.md`. Keep the custom
Select/Back menu, controls/settings, MStan performance and Steam Controller fixes.
Rewind stays enabled: hold Y/Triangle/F8, release resumes the selected state.
NAS and STEAMBOX are unrelated. Do not publish a release/feed.

Engine `b8a88cea` includes exact nearest-checkpoint resume, skipped redundant
hidden redraw/presentation, PBO texture restaging, PGXP read-journal optimization
and immutable Mac bundle storage. UI `4efe547` honors the host player-data root.
Release build and 41 CTests passed; supported regeneration reused all 7,459
variants without generated-C changes. Build: `build-shipping-20261111`.

Compatible isolated grid: `analysis/release-pgxp-grid-20261111/memcards`.
Execution identity `769ca791378dc7f82cab1af06580065bf515cbef79182c691f6f6e43c26c07fe`.
Never bypass its guard. Current exact-state/image proof:
`analysis/release-pgxp-proof-20261111/result.json` (three exact RAM restores and full-image parity passed).

## Release gates still in progress

- Verify final signed bundle's menu/settings persistence and hidden race smoke.
- Measure actual packaged foreground performance and repeated rewind releases.
- Notarize, staple, validate Gatekeeper and regenerate external delivery checksums.
- Push engine/UI/game commits after required checks pass.

Developer ID signing and Keychain profile `pp-notary` are verified. The owner
resolved Apple's account agreement requirement. Never expose credentials.
Player data belongs in `~/Library/Application Support/net.ramine.r4recomp`;
`PSX_BUNDLE_DATA_DIR` isolates checks. Resources stay inside the signed bundle.

Latest pre-PGXP foreground run: `analysis/release-upload-visible-20261111/result.json`:
57.65 FPS, all 3,465 intermediate frames rendered, 135–161 ms complete rewind
release. Prior exact proof restored three RAM states with zero differences and
77 matching full-resolution images. These are bounded measurements, not proof
of sustained perfect 60. Background work was active; do not stop unrelated jobs.

Internal rendering is 3834×2160; the physical monitor is 2560×1440 at75 Hz.
Tunnel/sky/ground/HUD samples showed no former pixelation or obvious seams/order
faults. Physical Steam Controller testing remains device-dependent. The separate
scene API's R4 producer is still unconnected; do not describe it as finished.

## References

- [Mac package workflow](MAC_BUILD.md)
- [Rewind architecture](REWIND_PLAYBACK.md)
- [Pin history](framework_pin_history.md)
- [Previous local playtest receipt](recovery/MAC_60FPS_20261111.json)
- [Renderer investigation](recovery/SEPARATE_RENDERER_20261111.md)
- [Threading evaluation](recovery/THREADING_EVALUATION_20261109.md)
- [Menu restoration](archive/CUSTOM_MENU_20261109.md) / [upstream](UPSTREAM.md)

Historical sources: `/Users/shokunin/dev-archives/r4-20261109/`.

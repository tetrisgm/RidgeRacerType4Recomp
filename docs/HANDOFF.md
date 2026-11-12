# Handoff

## Current work — 2026-11-11

Mac checkout on `master`; follow `CLAUDE.md`. NAS and STEAMBOX are unrelated.
Keep the custom Select/Back menu, settings/controls, MStan performance work and
Steam Controller fixes. The owner asked to wrap up rather than keep repeating
performance tests. Rewind remains core: hold Y/Triangle or F8, release to resume.

Engine `60aba7e8` adds exact ordered command rewind, native texture history,
private playback texture memory, fractional 60-Hz cadence, no unused thumbnails,
and four-frame machine anchors. Includes CRC batching `802f1771`. R4 opts in by
default in CMake; framework base remains selectable. UI stays `ef8150f5`.
Supported regeneration reused all 7,459 native variants without generated-C edits.

Build: `build-shipping-20261111` (arm64 Release/IPO/OpenBIOS, tracing off).
Release build and 39 CTests passed. Final isolated custom-menu/settings/race smoke:
`analysis/rewind-command-final-menu-20261111/native-overlay-check.json`.
Final default-backend run: `analysis/rewind-command-default-final2-20261111/result.json`.
The current grid is `analysis/60fps-20261111-command-anchors-grid/memcards`;
execution identity `732955a863cf162b31b127a282b22622cd07551b93a6a8b4329beff87fe7857e`.
Do not bypass state identity checks.

Local delivery: `dist/macos/2026.11.11-rewind1/R4.app`.
Scope is a local playtest with the limitations below, not a perfect-60 claim.

## Evidence and limits

- Exact release RAM and 24 full internal image comparisons passed:
  `analysis/60fps-20261111-command-release-proof/result.json`.
- Visible 120-second race: 58.92 forward FPS and 59.57 backward updates/s:
  `analysis/60fps-20261111-command-visible/result.json`.
- Subsequent 30-second run: 60.01 forward FPS and 58.99 backward updates/s:
  `analysis/60fps-20261111-command-release-profile/result.json`.
- Long-hold release took 0.38–0.51 seconds including observation polling. This
  brief pause is still a known limitation. Do not claim perfect sustained 60.
- Internal rendering is 2160 lines; physical display is 1440p/75 Hz. Visible
  tunnel/sky/ground/HUD samples show no former center pixelation or obvious
  seams/order faults. No all-course guarantee; physical Steam Controller testing
  remains device-dependent. Synthetic host input and menu/settings tests pass.

Command playback stores state/commands, not framebuffer history. Release keeps
the last selected complete image until the first complete forward producer;
native VRAM restages cannot leak to screen. The earlier independent scene API
exists, but its R4 producer remains unconnected: do not call it a finished
renderer built from scratch.

## References

- [Rewind architecture and evidence](REWIND_PLAYBACK.md)
- [Mac build/package workflow](MAC_BUILD.md)
- [Delivery receipt](recovery/MAC_60FPS_20261111.json)
- [Renderer investigation](recovery/SEPARATE_RENDERER_20261111.md)
- [Threading/history evaluation](recovery/THREADING_EVALUATION_20261109.md)
- [Menu restoration](archive/CUSTOM_MENU_20261109.md)
- [Upstream reviews](UPSTREAM.md) / [pin history](framework_pin_history.md)

Historical sources remain at `/Users/shokunin/dev-archives/r4-20261109/`.

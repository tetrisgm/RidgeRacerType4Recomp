# Custom menu restoration — 2026-11-09

The [history audit](../recovery/HISTORY_AUDIT_20261109.md) supplies the source
selection and limits. This implements its custom-menu restoration contract.

## Source and behavior

- Engine `d585a728` descends from `ae3eae8f`: MStan's upstream-based performance
  recovery, HD residency/coverage, XA rewind and Steam Controller fixes remain.
  Menu changes do not alter guest execution, timing or renderer algorithms.
- UI `ef8150f` restores custom recomp-launcher from `0b89a344`, with the
  original R4 title layer, current host schemas and ABI compatibility.
- Startup opens the game menu; Select/Back alone opens/closes it during play.
  Options contains display, controls, bindings, devices, deadzones and volume.
  Save/Load State uses the real runtime slot operations and thumbnails.
- Keyboard/gamepad bindings reload, device/deadzone edits apply, volume and
  settings persist. PGXP stays required. Restart-only edits remain pending
  instead of unexpectedly restarting a race on Resume.
- Supported game/OpenBIOS regeneration produced unchanged generated C;
  metadata/fingerprints reflect the new sources. All 7,459 native overlay
  variants remain linked. No old interpreter overlay path was reintroduced.

## Mac validation

Isolated evidence: `analysis/custom-menu-20261109/qualified-02/`.
The owner profile, native saves and explicit settings were not used.

- Release build and 29/29 CTests pass; custom UI core tests pass.
- Six custom menu fixtures pass: main, states, netplay restrictions, missing
  disc, Steam fixture and Add to Steam against that fixture.
- `tests/custom_menu.txt` passes over the actual hidden game: 5x resolution,
  Classic scheme, P2 keyboard, 25% deadzone, Cross bound to K and gamepad X,
  volume 53, and repeated Select/Back navigation.
- 40-step hidden/dummy-audio boot-to-race smoke reaches frame 8,469,
  records 7,180,068 static overlay hits, zero dispatch/segment misses and no
  interpreted overlay PCs. This is route coverage, not exhaustive coverage.
- Actual menu check confirms paused guest frames, repeated Select/Back and
  Guide, save then load slot 0, thumbnail rendering and resumed execution.
- Fresh process reload confirms 5x resolution, volume 53, P2 keyboard,
  deadzone 8191, keyboard scancode 14 and gamepad button 2. These are isolated
  test values, not changes to the owner's settings.
- A first run found a debug window-resize/render-readback race in OpenGL.
  The runtime now drains rendering before that resize; the full rerun passes.
- Windows x64 Release cross-build succeeds on the Mac. Packaging checks AMD64,
  system/UCRT imports, all linked overlay symbols and SHA-256 manifests.

Reproduce with `python3 tools/verify_native_overlays.py <new-output-dir>
--menu-script tests/custom_menu.txt`. Runtime receipts, screenshots and logs
remain local because they can contain game data. Portable delivery version:
`dist/windows/2026-11-09-custom-menu-01/`; `BUILD.json` records exact source pins.

Windows execution, physical Steam Controller/Steam Input, normal-paced rank-8
regression, all-course/netplay play and crowded-race 4K performance remain
owner qualification. Synthetic SDL inputs and Mac tests do not establish those.
NAS and STEAMBOX were not accessed.

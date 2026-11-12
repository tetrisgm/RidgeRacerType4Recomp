# Native Apple Silicon package

Build on the Mac checkout. The NAS and STEAMBOX are not build machines.

```sh
cmake --preset macos-native
cmake --build --preset macos-native
ctest --preset macos-native
python3 tools/verify_native_overlays.py analysis/mac-menu-NEW \
  --build build-macos-native --menu-script tests/custom_menu.txt --present-thread
python3 tools/perf/qualify_mac.py analysis/mac-race-NEW \
  --build build-macos-native --state /absolute/path/to/isolated/grid/memcards \
  --visible --borderless --rewind
```

The grid state must be slot 0 from a Grand Prix race. The performance harness
copies it, proves the car moves, measures 120 seconds at 3834×2160, retains
PGXP and records intermediate generations separately from VBlanks and swaps.
It inherits actual display timing by default; `--no-native-display` is an
explicit simulated-60-Hz diagnostic. The 60-FPS enhancement owns its swap
policy. The separate menu smoke disables VSync for hidden-window automation. Inspect successive native and
intermediate race captures and the live desktop separately; this measurement
alone does not establish physical-display cadence or all-course correctness.

Release tracing is disabled and IPO enabled. TCP verification is explicitly
opted in by the tools; it is not enabled for ordinary packaged play. The
presentation worker follows the game's default. No adaptive resolution or
lowered texture detail is used to meet the target.

After committing verified code, stage a fresh version with a qualification
receipt whose `binary_sha256` identifies that executable:

```sh
python3 tools/stage_macos.py VERSION --build build-macos-native \
  --race-60 --evidence /absolute/path/to/qualification.json \
  --sign-identity "Developer ID Application: YOUR NAME (TEAMID)"
```

This creates `dist/macos/VERSION/`, checks the 7,459 statically linked overlay
variants and system-only dependencies, signs the whole app with hardened runtime
and a timestamp when a Developer ID is supplied, binds its external execution
manifest and verifies SHA256SUMS. Omitting the identity produces only an ad-hoc
local build. It includes OpenBIOS and licenses;
no disc, retail BIOS, saves, settings from the owner, SDK or runtime compiler.
`--race-60` selects fixed 9× resolution and the GP 60-FPS enhancement in the new
package only, and enables automatic rewind capture, a required release feature.
Rewind remains available in the menu; manual save/load states are unaffected.
Launch `R4.app` (or `Start R4.command`); the integrated menu provides
controls/settings and opens during play with Select/Back. Immutable resources live under `Contents/Resources/runtime`; saves and settings
live in `~/Library/Application Support/net.ramine.r4recomp`. An existing portable
folder is left intact. `PSX_BUNDLE_DATA_DIR` selects an absolute isolated data
folder for verification. Player edits are preserved across default updates.

Verify the actual signed bundle before notarization:

```sh
python3 tools/verify_native_overlays.py analysis/mac-bundle-NEW \
  --build build-macos-native --app dist/macos/VERSION/R4.app \
  --menu-script tests/custom_menu.txt --present-thread
codesign --verify --deep --strict dist/macos/VERSION/R4.app
```

Notarization uses an existing Keychain profile; never put credentials in command
arguments. Submit a `ditto` ZIP with `xcrun notarytool submit --wait`, require
Accepted, staple the app, then validate with `stapler`, `codesign` and `spctl`.
Update external `DELIVERY.json` and regenerate checksums after stapling. Create
the distributable archive from the stapled app. Staging does not publish a
release or feed.

See [current qualification](recovery/MAC_60FPS_20261111.md) for measured scope
and remaining physical-display verification.

The app declares the racing-game category and the supported Game Mode keys
([Apple reference](https://developer.apple.com/documentation/bundleresources/information-property-list/lssupportsgamemode)).
This makes it eligible for macOS foreground game scheduling; a plist declaration
alone is not proof Game Mode activated or that a frame-rate target passed.
`qualify_mac.py --app /absolute/path/to/R4.app` measures that actual bundle with
isolated player data, using the same race and rewind checks.

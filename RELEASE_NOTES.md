# Ridge Racer Type 4 Recompiled — v0.2.2

Rewind release now restores the rendering work already issued at the selected
state, eliminating the extra pipeline rebuild. Two full-resolution working
images avoid redundant drawing; rewind history remains commands and state in
bounded memory. Safe checkpoint anchors shorten the remaining state replay.

The custom Select/Back menu, controls/settings, MStan performance work and
Steam Controller fixes remain intact. PGXP and fixed 2160-line rendering stay
on, including during hold-to-rewind. The Mac render worker now has a private
hidden drawable to avoid competing with the visible presentation context.

The two-minute native Mac race averaged 59.86 VBlanks/s and rendered all 3,599
requested intermediate frames. Five internal releases took 27–47 ms (31–65 ms
including input observation), versus the previous 99–129 ms transition. Three
restores had zero RAM differences; all 130 full-resolution comparisons matched.
The final version-stamped build check measured 35–55 ms internally.
These measurements cover a finite route, not every course or a locked-60 promise.
Tests are muted; listening and physical-controller checks remain separate.

Mac Apple Silicon and portable Windows x64 packages require your own USA disc.
No game data, retail BIOS, developer tools or player saves are bundled.
The Mac app is Developer-ID signed, notarized and stapled. Windows remains
unsigned. The native RTX 4080 SUPER test averaged 59.96 VBlanks/s, rendered all
3,598 intermediate frames, and passed three exact restores with 108 matching
images. Internal PC releases took 42–72 ms. Select/Back menu checks passed on
both platforms. Tunnel, sky, ground and resume captures showed no observed
seams, blinking artifacts or displaced HUD. See the current qualification
receipts for the measured scope.

---

# Ridge Racer Type 4 Recompiled — v0.2.1

The custom menu is integrated into the game, with full-resolution hold-to-rewind
and the MStan performance and Steam Controller fixes retained.

## What changed

- **Select/Back opens the game menu.** Controls, bindings, display, audio and
  other settings are available there. Esc and Guide also open it.
- **Hold Y/Triangle/F8 to rewind; release to resume.** Rewind redraws at the
  gameplay resolution and restores the selected machine state. It does not
  open a separate rewind menu or switch to a low-resolution preview.
- **Lower rewind overhead.** Memory history, command replay, GL context
  ownership and texture uploads avoid the earlier large capture/release stalls.
- **Preserved rendering quality.** PGXP, widescreen and the native renderer
  remain enabled. Fresh packages select fixed 9× (2160-line) rendering and
  the 60-FPS Grand Prix enhancement, with rewind enabled.
- **Persistent controls on both platforms.** Mac player files live outside
  the signed app. Windows now correctly replaces existing binding files.

## Install

The prepared packages are macOS Apple Silicon and Windows x64. They contain
OpenBIOS and the compiled runtime; no disc, retail BIOS, compiler or SDK is
included. Use your own verified USA BIN/CUE. This release does not include a
new Intel Mac package.

On Mac, open **R4.app** and select your disc. The app is Developer-ID signed,
Apple-notarized and stapled. It can live in Applications. Player data is in
`~/Library/Application Support/net.ramine.r4recomp/`; preserve existing saves
and settings when moving from a portable installation.

On Windows, extract the portable ZIP into a new writable folder and open
`r4-runtime.exe`. Preserve the previous folder as a backup. The Windows EXE is
unsigned; hardened Defender reputation policies can block a new build.
The package does not alter security policy or install system services.

## Verification and limits

The Apple M4 visible race ran at 3834×2160 internally for two minutes with
rewind enabled: **59.98 VBlanks/s**, all **3,605** requested intermediate frames
rendered, and no gameplay guest-frame interval above 50 ms. Rewind releases
resumed in **99–129 ms**. Tunnel, sky, road and rewind sequences showed no
observed blinking, polygon seams, displaced HUD or resolution drop; the owner
accepted the visual pass. This is a measured route, not a locked-60 guarantee
for every course or machine.

All 44 Mac tests and menu persistence checks pass. Exact rewind RAM restoration
and full-resolution image comparisons pass on Mac and Windows-on-CrossOver.
The native RTX 4080 SUPER PC ran the 4K drawable for two minutes at **59.96
VBlanks/s**, with no sampled presentation interval above 19 ms. Select/Back
menu checks and two exact rewind restores passed; 52 full-resolution images
matched. Windows remains unsigned; the hardened test PC required an owner-
approved temporary ASR exception, removed after the test. Hands-on audio, gameplay and
physical Steam Controller evaluation remains separate from automated input.

Wheel force feedback remains experimental and off by default. The existing
rare four-player Link Battle Results-screen halt and Extra Trial limitations
remain outside this release's tested route.

---

# Ridge Racer Type 4 Recompiled — v0.2.0

## The download is now the compiled game

No more setup kit and no first-run build: unzip and run `r4-runtime`. The game
starts straight away. On first run it looks for your own Redump bin/cue in the
usual places (the game folder, Games, Downloads, Documents, Desktop, ROM and
Emulation folders, and removable drives) and
checks it against the Redump hashes. If it finds nothing, the in-game menu's
**Disc setup** page lets you pick it. Nothing is downloaded or compiled before
you can play. The zip holds no disc data and no BIOS dump; the game runs on the
bundled OpenBIOS (or on your own SCPH-1001 dump, if you pick one).

Downloads: macOS Apple Silicon (`macos-arm64`), macOS Intel (`macos-x64`),
and Windows x64.

Updating from v0.1.0: extract into a new folder, or over the old one. Memory
cards and settings carry over; savestates from v0.1.0 are refused.

## The menu lives in the game

Esc, Guide or Start+Select opens the menu over the game: Graphics, Controls,
Mods, Netplay, Disc setup and more. The game pauses while it is open (in
netplay only your own inputs are held). Most settings apply at once; the ones
marked "Applies after restart" relaunch the game when you close the menu.

## Graphics presets

The first start picks a preset for your machine: **Low** on integrated GPUs
and base Apple M1/M2, **Ultra** elsewhere. Graphics lists
Low, Medium, High and Ultra; changing any setting a preset governs makes it
**Custom**. Low keeps PGXP and turns the extras off (bloom, Smooth motion and
supersampling) to reduce load on an iGPU.

## New, and on by default

- **Modern controls.** On a gamepad with analog triggers: RT gas, LT brake,
  left stick steers, Square / Circle shift down / up, R1 changes the view,
  Y opens Rewind. Menus and the pause menu keep the stock buttons. Want the
  original? Mods → Controllers → Controls → **Classic**.
- **Camera look-around.** The right stick turns the view in single-player
  races.
- **Rear-view mirror hidden.** The mirror inset is skipped for a clearer view;
  switch **Hide Rear-view Mirror** off in Mods to bring it back.
- **JogCon and analog steering.** Recognized steering wheels drive as R4's
  own JogCon; gamepads use native DualShock analog steering.
- **PGXP.** No more wobbling polygons or bent textures, and the far road is
  drawn without gaps.
- **Max Detail.** Longer draw distance, full-detail course and cars at every
  distance, 1P detail in split screen and reflective cars in the race. Every
  part can be set back to Stock.
- **Faster VS split screen.** 2P split screen takes about a tenth of the draws
  on OpenGL.
- **Online battle for 2-4 players.** R4's Link Battle over LAN or the
  internet: the host opens a room on the NETPLAY page, the others join, and
  everyone races on their own screen with their own full-screen view.
- **Widescreen.** Races fill the window at whatever shape you give it (Fit
  to Window), with real extra scenery at the sides and the HUD at the edges.
- **Sharper picture.** The game matches your display up to 8K and uses 1.5×
  supersampling below 4K. Dynamic resolution is off by default; when enabled
  it never goes below 1080 lines.
- **Race frame rate.** Auto by default (60 FPS; the original 30 at 8K/18x),
  with 30, 60, 90, 100, 120, 144 and Match display refresh choices. Workers draw the extra race frames while
  game logic, input and audio keep their original 30 Hz cadence. The achieved
  rate depends on resolution and GPU load; 144 FPS is not sustained on the
  current 1440p test PC under the measured load.
- **PGXP extras.** A depth buffer (no sorting glitches where polygons cross),
  smooth shading and closed seams between polygons.
- **HD HUD.** The race HUD drawn from high-resolution artwork: **HD HUD by
  Kuid0us** (github.com/Kuid0us/T4HDHUD), re-keyed for the US disc.
- **Rewind is off in split screen and online,** so it can't put one player out
  of step. It still works in single-player races.

- **Wheel force feedback (experimental, off by default).** A force-feedback
  wheel presented as R4's JogCon can drive its motor from the game: Controls →
  **Wheel force feedback (experimental)**. Gamepads are never driven. It has
  not been verified on a physical wheel yet; feedback is welcome.

Every feature above has a switch on the Mods page or under Graphics; switching
it off restores the stock game for that part.

## Known issues

- **Wheel force feedback is experimental** and off by default. Its direction
  and strength are not verified on a physical wheel.
- **4-player online: rare halt on the Results screen.** Very rarely a 4-player
  Link Battle halts on the Results screen after the race
  (RetroPortingToolKit/psxrecomp#636, under investigation).
- macOS builds are not notarized: run
  `xattr -dr com.apple.quarantine <folder>` once, or use **Open Anyway** in
  System Settings → Privacy & Security.
- Intel Macs: use `macos-x64`. On Apple Silicon take `macos-arm64`; the x64
  build under Rosetta runs the code-overlay menus in the interpreter.

## Credits

R4 recompilation by Shokunin, built on psxrecomp, recomp-ui and recomp-launcher
(RetroPortingToolKit), with the MIT-licensed OpenBIOS from PCSX-Redux.

- HD HUD artwork: **HD HUD by Kuid0us** (github.com/Kuid0us/T4HDHUD),
  re-keyed for the US disc.
- Menu font: Lato (SIL Open Font License 1.1).
- Menus: Dear ImGui (MIT).

R4: Ridge Racer Type 4 is © Bandai Namco Entertainment; the zips contain none
of its disc data. Every third-party notice is in `THIRD-PARTY-LICENSES/` and
`licenses/`.

---

# Ridge Racer Type 4 Recompiled — v0.1.0 (preview)

R4: Ridge Racer Type 4 (USA) statically recompiled to native code with
psxrecomp. **This download is a setup kit, not a playable game**: it contains
no game code, no BIOS and no disc data. On first run it builds the game on your
computer from **your own disc**.

## What you need

- Your own **R4: Ridge Racer Type 4 (USA)** disc image as the Redump **bin/cue**
  (serial SLUS-00797, one track). Hashes are in `DISC.md`. Do not convert it to
  `.iso`: that drops the streamed music and movies.
- An internet connection for the first build (it downloads the build tools and
  a few libraries).
- About 2 GB of free space and 10–20 minutes for the first build.
- **macOS 11 or later** (Apple Silicon; Intel is included but only smoke-tested):
  Apple's Command Line Tools.
  If you don't have them, open Terminal and run `xcode-select --install`.
- **Windows 10/11 x64**: nothing else.

## Install

1. Download the zip for your system and extract it to a folder with a short
   path, e.g. `C:\Games\R4` or `~/Games/R4`.
2. **macOS only — allow the app to open.** This preview is not notarized, so
   macOS blocks it ("Apple could not verify…"). Open Terminal and run, once:

       xattr -dr com.apple.quarantine ~/Games/R4

   (use your folder's path). Alternatively, after each blocked launch, click
   **Open Anyway** in **System Settings → Privacy & Security** (macOS 13 and
   later) or **System Preferences → Security & Privacy → General** (macOS 11
   and 12).
3. **Windows only:** if SmartScreen says "Windows protected your PC", click
   **More info → Run anyway**.
4. Run `r4-runtime` (`r4-runtime.exe` on Windows). The setup wizard opens:
   - **Build tools:** download them (≈100 MB on macOS, ≈200 MB on Windows).
   - **Disc:** pick your `.cue`. It is checked against the Redump hashes.
   - **Generate & rebuild:** generates the game code and compiles it.
5. When it finishes the game starts. From then on, `r4-runtime` opens the game
   directly.

## Playing

- In R4's menus **Circle is OK** and **Cross is cancel**; in races Cross
  accelerates by default. Controls are configurable in the launcher.
- **2 players:** VS Battle is split screen. Play it locally with two
  controllers, or online from the launcher's **Netplay** page (both players
  need this same version and the same Redump disc).
- Saves are standard PS1 memory cards in `saves/`.

## Known limitations

- Link-cable battle is not supported.
- Recognized SDL steering wheels now use an emulated JogCon SIO identity and
  signed steering position. Ordinary gamepads default to R4's native DualShock
  analog protocol. The JogCon motor command is exposed in emulated device state;
  wheel force feedback is not translated or hardware-verified. NeGcon emulation
  remains unavailable in the framework.
- The first visit to some menus (garage, car select, records) runs slower while
  their code is compiled in the background; later visits are native.
- Updating: extract a new version to a **new folder** (extracting over an old
  one keeps running the old build), then copy `saves/` — and
  `build-release/settings.toml` to keep your settings and controls — from the
  old folder into the new one before its first launch.
- Not yet tested through a full Grand Prix season.

## Credits

Built on [psxrecomp](https://github.com/RetroPortingToolKit/psxrecomp) and
[recomp-ui](https://github.com/RetroPortingToolKit/recomp-ui), with the
MIT-licensed OpenBIOS from PCSX-Redux. R4: Ridge Racer Type 4 is © Bandai Namco
Entertainment; this kit contains none of its code, audio, video or game data.
The launcher shows the retail cover art (from
[libretro-thumbnails](https://github.com/libretro-thumbnails/libretro-thumbnails))
to identify the game.

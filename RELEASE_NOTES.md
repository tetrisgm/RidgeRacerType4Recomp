# Ridge Racer Type 4 Recompiled — vNEXT (draft)

> Draft for the next release. The version number is the owner's call; this
> section is not published yet.

## The download is now the compiled game

No more setup kit and no first-run build: unzip, run `r4-runtime`, pick your
own Redump bin/cue. Nothing is downloaded or compiled before you can play. The
zip holds no disc data and no BIOS dump; the game runs on the bundled
OpenBIOS (or on your own SCPH-1001 dump, if you pick one in Settings).

Updating from v0.1.0: extract into a new folder, or over the old one. Memory
cards and settings carry over; savestates from v0.1.0 are refused.

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
- **Sharper picture.** The game renders at your monitor's full resolution,
  1.5× supersampled, and dynamic resolution keeps it at full speed (it never
  drops below your display's own resolution).
- **Smooth motion.** In-between frames up to your display's refresh rate,
  variable-refresh displays included; the game itself still runs at 30 Hz.
  It replaces the old R4 Frame Rate mod.
- **PGXP extras.** A depth buffer (no sorting glitches where polygons cross),
  smooth shading and closed seams between polygons.
- **Rewind is off in split screen and online,** so it can't put one player out
  of step. It still works in single-player races.

Every feature above has a switch on the Mods page or under Settings →
Display; switching it off restores
the stock game for that part.

## Coming next

The HD HUD pack.

## Credits

R4 recompilation by Shokunin, built on psxrecomp and recomp-ui
(RetroPortingToolKit). Third-party notices are in `THIRD-PARTY-LICENSES/`.

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

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

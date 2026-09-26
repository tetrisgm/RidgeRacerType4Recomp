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
- **macOS 11 or later** (Apple Silicon or Intel): Apple's Command Line Tools.
  If you don't have them, open Terminal and run `xcode-select --install`.
- **Windows 10/11 x64**: nothing else.

## Install

1. Download the zip for your system and extract it to a folder with a short
   path, e.g. `C:\Games\R4` or `~/Games/R4`.
2. **macOS only — allow the app to open.** This preview is not notarized, so
   macOS blocks it ("Apple could not verify…"). Open Terminal and run, once:

       xattr -dr com.apple.quarantine ~/Games/R4

   (use your folder's path). Alternatively open **System Settings → Privacy &
   Security** and click **Open Anyway** after each blocked launch.
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
- NeGcon and JogCon are not supported; use a DualShock/analog pad or keyboard.
- The first visit to some menus (garage, car select, records) runs slower while
  their code is compiled in the background; later visits are native.
- Updating: extract a new version to a **new folder**. Extracting over an old
  one keeps running the old build.
- Not yet tested through a full Grand Prix season.

## Credits

Built on [psxrecomp](https://github.com/RetroPortingToolKit/psxrecomp) and
[recomp-ui](https://github.com/RetroPortingToolKit/recomp-ui), with the
MIT-licensed OpenBIOS from PCSX-Redux. R4: Ridge Racer Type 4 is © Bandai Namco
Entertainment; this project contains none of its code or assets.

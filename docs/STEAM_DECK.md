# Steam Deck and Linux

The Linux release (`r4-<version>-linux-x64.zip`) is one x86_64 build that runs
on SteamOS 3 (Steam Deck) and on any desktop distribution with glibc 2.31 or
newer. It needs nothing installed:

- SDL3 is built in.
- The overlay toolchain (Python, the recompiler, TinyCC and the libc headers it
  needs) ships in `overlay_toolchain/`. Code the game streams from the disc
  therefore becomes native code even on a Deck, which has no compiler.

## Install on a Steam Deck

1. In Desktop Mode, unzip the release anywhere in your home folder, for example
   `~/Games/R4/`. Put your disc image (Redump `.cue` + `.bin`, see `DISC.md`)
   next to it or anywhere else.
2. Run `r4-runtime` once from Desktop Mode. The launcher opens: pick your disc
   image, then start the game. It saves the choice in `settings.toml` beside the
   executable.
3. In Steam, choose **Add a Non-Steam Game** and select `r4-runtime`.
4. Switch to Game Mode and start it from your library. In Game Mode
   (gamescope), R4 skips the launcher and opens fullscreen (psxrecomp
   `docs/GAME_MODE.md`). To reach the launcher's settings from Game Mode, add
   `--launcher` to the shortcut's launch options for one start.

## Controls

Use Steam Input's default **Gamepad** template. R4's Modern controls (on by
default) are built for exactly that layout:

| Deck control | R4 (Modern controls) |
| --- | --- |
| R2 (right trigger) | Gas, analog |
| L2 (left trigger) | Brake, analog |
| Left stick | Steering, analog |
| X / B (Square / Circle) | Shift down / up (manual transmission) |
| R1 | Change view |
| Y (Triangle) | Rewind |
| Start | Pause |
| A (Cross) | Confirm in menus |

Recommended additions in the controller settings for the shortcut:

- **L4 → X** and **R4 → B**: paddle shifters on the back grips.
- **Right trackpad → Mouse** (default): handy in the launcher, unused in races.

If you prefer R4's stock controls (Cross accelerates, Square brakes), turn
off **Mods → Controllers → Controls** in the launcher, or pick **Classic**
there. A steering wheel is detected as a JogCon on its own.

## Graphics

With graphics presets (tetrisgm/RidgeRacerType4Recomp#23), R4 picks a preset
from the hardware the first time it starts. A Steam
Deck gets **Low**: no supersampling, no Smooth motion, and no PGXP
depth/colour/seam extras (PGXP itself stays on). Low is sized for 60 Hz at the
Deck's 800-line display, based on measurements of slower proxies; it has not
been timed on a Deck yet. Settings → Display → **Graphics preset** shows what was
detected. Pick another preset there, or **Re-detect**.

## Building the Linux zip

Build inside the Steam Runtime 3 "sniper" SDK image (glibc 2.31, GCC 14), on
x86_64. On an Apple silicon Mac, use a Rosetta-backed amd64 container:

```sh
docker run --platform linux/amd64 -v "$PWD:$PWD" -w "$PWD" \
  registry.gitlab.steamos.cloud/steamrt/sniper/sdk:latest \
  bash -c 'export CC=gcc-14 CXX=g++-14 R4_PYTHON=<python 3.11+>; tools/package_release.sh HEAD'
```

`tools/package_release.sh` refuses the zip in these cases:

- the executable imports anything but glibc's own libraries;
- it needs a glibc symbol newer than 2.31;
- the bundled TinyCC, its musl headers or their notices are missing;
- an executable lost its `+x` bit.

The packager needs Python 3.11 or newer (`R4_PYTHON`). Sniper's own Python is
3.9.

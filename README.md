# RidgeRacerType4Recomp

<p align="center">
  <img src="recomp/launcher/boxart.png" alt="R4: Ridge Racer Type 4 box art" width="280">
</p>

> _In-development preview, not a finished port — expect rough edges._

R4: Ridge Racer Type 4 (USA, SLUS-00797) statically recompiled to a native
executable with [psxrecomp](https://github.com/RetroPortingToolKit/psxrecomp)
and [recomp-ui](https://github.com/RetroPortingToolKit/recomp-ui), structured
after [MegaManX6Recomp](https://github.com/mstan/MegaManX6Recomp).

## What This Is

The game's MIPS code is machine-translated ahead of time into C, then compiled
into a native program that runs the game's own logic on psxrecomp's simulation
of the PS1 hardware (GPU, SPU, GTE, MDEC, CD-ROM, pads, memory cards) and the
real, recompiled PS1 BIOS — no high-level emulation shims.

This repository holds the game-specific configuration, seeds, tools and build
glue, plus `generated/`: the boot EXE's code machine-translated to C,
committed so that a release can ship the compiled game. The C keeps every
original MIPS instruction word next to its translation (see License). The
repository does **not** contain the disc image, the game's data, a BIOS dump,
or any code derived from a retail BIOS. Builds use the MIT-licensed OpenBIOS
from PCSX-Redux; bring your own legally obtained disc.

Important files:

- `game.toml`: identity, disc digests, recompiler/runtime/video/controller/netplay config.
- `seeds/`, `annotations/`, `symbols.toml`: recompiler inputs grown from RE work.
- `generated/`: the recompiled game C (committed; `tools/regen.sh` rewrites it).
- `tools/regen.sh`: regenerate the game C from the disc.
- `tools/package_release.sh`, `scripts/package_release.sh`: build the release zip.
- `tools/run_r4.sh`, `tools/dbg.py`, `tools/pad.py`, `tools/smoke.py`: run and drive a debug build.
- `src/mods/`, `mods/preloaded/`: game-owned mods (widescreen); see `docs/WIDESCREEN.md`.
- `DISC.md`: Redump-verified disc identity. `ISSUES.md`: issue log.
- `docs/framework_pin_history.md`: why each submodule pin moved.

## Status

**Bring-up preview.** Boots, plays races, and runs 2-player VS Battle over
netplay. Not yet verified end to end (see `ISSUES.md`).

| Area | State |
|---|---|
| BIOS boot | Works — recompiled OpenBIOS, HLE boot-skip and full LLE intro |
| Intro / attract movies (MDEC + XA) | Play; Start skips after the Namco logo |
| Menus, Grand Prix setup, race | Work |
| Audio (SPU + XA music) | Works |
| Code overlays (R4.BIN) | Captured and compiled to native shards in the background |
| VS Battle (2P split screen) | Works over netplay (delay-sync and rollback, digests match) |
| Link battle (link cable) | Not supported (no SIO1 model) |
| Renderer | Stock psxrecomp OpenGL at 4:3; software selectable |
| Internal resolution | Native to 8K presets (Settings → Display), OpenGL |
| Widescreen | Mods > Display > R4 Custom Renderer (experimental, off by default): native-wide races, Fit to Window / 16:9 / 21:9 / 32:9 |

## Playing a release

A release zip is the compiled game. Nothing is generated or built on your
computer, and the zip holds no disc data and no BIOS dump: you bring the disc.

You need:

- Your own **R4: Ridge Racer Type 4 (USA)** disc image as the Redump
  **bin/cue** (SLUS-00797, one track; hashes in `DISC.md`). Do not convert it
  to `.iso`: that drops the streamed music and movies.
- macOS 11 or later (`macos-arm64` for Apple Silicon, `macos-x64` for Intel),
  or Windows 10/11 x64.
- Optional on macOS: Apple's Command Line Tools (`xcode-select --install`).
  With them, the menus R4 loads as code overlays (garage, car select, course
  info, records) are compiled to native code in the background on your first
  visit; without them those menus run in the interpreter, which is slower
  but works. Windows needs nothing: the zip carries its own compiler.

1. Extract the zip to a folder you can write to, e.g. `~/Games/R4` or
   `C:\Games\R4`. Saves (`saves/`) and settings are kept beside the game.
2. macOS: this build is not notarized, so macOS blocks it the first time. Run
   `xattr -dr com.apple.quarantine ~/Games/R4` once (your folder's path), or
   click **Open Anyway** in System Settings → Privacy & Security after a
   blocked launch. Windows: if SmartScreen warns, choose **More info → Run
   anyway**.
3. Run `r4-runtime` (`r4-runtime.exe`). On first run, **Browse Disc** and pick
   your `.cue`; it is checked against the Redump hashes and never copied. Then
   **Continue to launcher** and **Play**.

The game runs on the bundled OpenBIOS. To use your own SCPH-1001 dump
instead, pick it under Settings → System → BIOS: the game compiles a backend
for it from your dump once (about a minute; on macOS this needs the Command
Line Tools) and uses it from then on. Widescreen and frame rate are under
**Mods**, internal resolution under **Settings → Display**; all are off by
default.

To update, extract the new zip over the old folder. Memory cards and settings
carry over. Savestates from an older version are refused, and code overlays
are compiled again on first visit.

## Building From Source (macOS, Windows)

Requirements: Xcode command-line tools, `brew install cmake ninja python`,
and R4: Ridge Racer Type 4 (USA, SLUS-00797) as the Redump bin/cue (verify
against `DISC.md`). Do not convert it to a 2048-byte `.iso`: that drops the
Mode-2 Form-2 XA sectors the music and movies stream from. Linux and Windows
follow `psxrecomp/docs/BUILDING.md`.

```sh
git clone --recurse-submodules <this repo> && cd ridgeracertype4
mkdir -p disc   # put (or symlink) the .cue and .bin here
# emitters for the dev overlay compiles tools/run_r4.sh sets up
bash psxrecomp/tools/ci/build_emitters.sh --framework psxrecomp --build-dir build-recompiler
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DPSX_DEBUG_TOOLS=ON
cmake --build build --target psx-runtime
tools/run_r4.sh build       # straight into the game; or build/r4-runtime for the launcher
```

The game C in `generated/` is committed, so a build needs no generate step.
After changing seeds, annotations, `[recompiler]` config or the `psxrecomp`
pin, regenerate it and commit the result:

```sh
tools/regen.sh --disc "disc/R4 - Ridge Racer Type 4 (USA).cue"
```

`tools/regen.sh` builds the emitters into `build-recompiler/`, verifies the
disc against `game.toml [prepare_disc]`, extracts the boot EXE to `disc/`, and
rewrites `generated/`. The recompiled BIOS backends come with the `psxrecomp`
submodule. Drop `-DPSX_DEBUG_TOOLS=ON` for a build without the TCP debug
server. The first configure downloads the pinned static SDL3 on macOS (libjuice
for netplay is vendored). Add `-DR4_BUILD_TESTS=ON` to register the developer
tests (`tests/`, `tools/`), then run `ctest --test-dir build`. The
widescreen cull-site check among them reads the disc's `.bin` in `disc/`; the
configure warns when it is missing.

Code overlays (R4.BIN menus) compile to native shards in the background only
when `PSX_OVERLAY_AUTOCOMPILE_CMD` is set; `tools/run_r4.sh` / `run_r4.cmd`
set it for dev runs, using the emitters in `build-recompiler/` (the quick
start builds them, and so does `tools/regen.sh`; `tools/run_r4.sh` says when
they are missing). Launching `build/r4-runtime` directly works, but those
menus stay in the interpreter.
Release zips ship an `overlay_toolchain/` instead, which is why `game.toml`
does not carry a dev compile command.

**Windows** builds the same way from an MSYS2 **MINGW64** shell
(`pacman -S mingw-w64-x86_64-{gcc,cmake,ninja,python} git`): run the same
`cmake` commands, then start `build\r4-runtime.exe`, or `tools\run_r4.cmd`,
which also puts MinGW64 on PATH so background overlay compiles can find
`python3` and `gcc`. The exe imports only Windows system DLLs.

### Packaging a release

`tools/package_release.sh [git-ref]` builds the release zip(s) from a
throwaway clone of a commit: `dist/r4-<version>-macos-arm64.zip` and
`-macos-x64.zip` on a Mac (one universal, ad-hoc signed build), or
`-windows-x64.zip` from an MSYS2 MINGW64 shell. It links the committed
`generated/` C and only the OpenBIOS backend, packages with psxrecomp's
`tools/package_game_release.sh` (through `scripts/package_release.sh`), and
checks every zip: the game, both R4 mods, `overlay_toolchain/` (the two
emitters, runtime headers and a Python runtime the game compiles overlays
with) and the third-party notices are in it; R4 and framework sources,
generated C, emitters at the root, disc data and BIOS images other than
OpenBIOS are not. Releases are built locally; there is no CI workflow.

## Configuration

Most options are in the launcher and persist to `settings.toml` beside the
executable. Defaults live in `game.toml`:

- `[video]` — `renderer` (`opengl` / `software`), `aspect_ratio = "4:3"`,
  `resolution_reference_lines = 240` (see Internal resolution).
- `[controller]` — `default_mode` (`digital`; DualShock analog selectable).
- `[runtime]` — `disc_speed = "1x"` (authentic; R4 streams XA with a data
  channel), `overlay_cache` (native overlay shards; see Building From Source).
- `[netplay]` — disc gates: `require_cue`, `required_tracks = 1`, `required_disc_fp`.

## Frame rate (optional mod)

Mods -> Frame Rate -> **R4 Frame Rate** (experimental, off by default) shows
races at the display's refresh rate or at 60 / 100 / 120 / 200 / 240 / 300 FPS.
The game itself still runs at its original 30 Hz: lap times, AI, input and
music are unchanged.

- **Interpolated** (default): the race is redrawn between game frames with
  the cars and camera part of the way to the next frame, by the game's own
  draw code inside a psxrecomp render pass (frozen guest time, everything
  restored afterwards). No added latency. Grand Prix and Time Attack races,
  the attract demo and the replay after a Time Attack are interpolated; VS
  split screen (not yet tested), menus, pause, results and movies are shown
  as on a PS1. Where the renderer cannot draw in-between frames at all, or
  more than a quarter of the last second's frames get none in time (e.g. at
  a high internal resolution), the package falls back to Frame blend and
  says so in the log; it returns once at most a tenth of them would miss out.
- **Frame blend**: crossfades finished frames (cheaper, ghosts, one frame
  late).

It needs the OpenGL renderer and turns vsync off. A monitor shows at most its
own refresh rate, so rates above it cost more without showing more motion
(with vsync off they can show as tearing instead).
If the machine cannot draw every in-between frame, fewer are drawn and the
gaps between them crossfaded; in-between frames are planned into the time
the presenter would otherwise wait. Netplay sessions run without mods.
Details and credits:
`mods/preloaded/packages/r4.enhancement.frame-rate/1.0.0/README.txt`,
`src/mods/r4_interp.c`, `psxrecomp/docs/RENDER_PASSES.md`.

## Internal resolution

**Settings → Display → Internal resolution** renders the game at a higher
resolution instead of stretching 320×240 (OpenGL). It is off (Native) by
default and takes effect when the game starts.

| Preset | Scale | Race frame | Notes |
|---|---|---|---|
| Native | 1× | 320×240 | Unchanged |
| 720p | 3× | 960×720 | |
| 1080p | 5× | 1600×1200 | Resolved down to the window |
| 1440p | 6× | 1920×1440 | |
| 4K | 9× | 2880×2160 | |
| 5K | 12× | 3840×2880 | |
| 8K | 18× | 5760×4320 | Past a 16384 GPU texture limit (Apple GPUs) only the displayed frame is kept at 8K |
| Match display | monitor height ÷ 240 | | Your monitor's pixel height |

- The menus are 480-line screens, so they render at twice the target and are
  resolved down; movies are unchanged.
- Textures stay the game's own; edges, geometry and the rear-view mirror get
  sharper. Wobbling polygons are the PS1's integer vertex snap, magnified.
- On a Mac, any preset above Native gives the game window a Retina (full
  pixel density) drawable.
- The GPU can lower a preset it cannot hold; the log line and the `video_info`
  debug command show the scale actually used. On an Apple M4 the 4:3 race
  measured 58.8 guest frames/s at 8K (60 is full speed) in a debug-tools
  build with its per-frame readback off; with widescreen, 8K is slower (32:9
  about 45). A release build was not measured.
- At 8K on a GPU with a 16384 texture limit (Apple GPUs), widescreen's Fit
  to Window goes up to about 34:9; a wider window is pillarboxed.
- Netplay: your own view only. Other players are unaffected and may use a
  different setting. Widescreen and frame-rate options, when present, are mods
  and are turned off for netplay; internal resolution is not.
- `PSX_INTERNAL_RESOLUTION=4k` (or any preset id, or a number of lines)
  overrides the setting for one run. `tools/res_matrix.py` checks every preset
  against a savestate.

## Controls

Keyboard and SDL gamepads per recomp-ui's input settings. In R4's menus
**Circle is OK** and **Cross is cancel**; in races Cross accelerates by default.

## Netplay

Two players, one per controller port: VS Battle's split screen. Use the
launcher's NETPLAY page (lobby, LAN, or Direct IP; rollback by default). Both
players need the same build and the same Redump dump — the `[netplay]` gates
refuse a mismatched disc. For a local two-instance test from the command line:

```sh
PSX_NET_MODE=rollback build/r4-runtime --no-launcher --netplay --net-slot 0 \
  --net-bind 127.0.0.1:7777 --net-session-id 1 --memcard-dir /tmp/p1 --debug-port 4797
PSX_NET_MODE=rollback build/r4-runtime --no-launcher --netplay --net-slot 1 \
  --net-bind 127.0.0.1:7778 --net-peer 127.0.0.1:7777 --net-session-id 1 \
  --memcard-dir /tmp/p2 --debug-port 4798
```

Details: `psxrecomp/docs/NETPLAY.md`.

## Memory Cards

Standard PS1 `.mcd` images in `saves/`, compatible with common emulators. Local
only; never commit them.

## Overlay cache

R4 streams code overlays from `R4.BIN`. The runtime records visited overlays in
`overlay_captures.json` and compiles native shards into `cache/` beside the
executable. **Do not publish `overlay_captures.json`** — it contains verbatim
snapshots of the game's code.

## Development Rules

- Real recompiled BIOS and hardware simulation; no HLE shims, no stubs, no
  hand-edited `generated/`.
- Framework fixes go to `psxrecomp`, not here. Resolve dispatch misses first
  (`tools/smoke.py` reports segment misses beside them).
- Disc images, BIOS dumps, memory cards, Ghidra databases and build outputs
  stay local. `generated/` is committed: regenerate it, never hand-edit it.
  See `CLAUDE.md`.

## License

MIT for this repository's own code — see `LICENSE`. Files adapted from
MegaManX6Recomp (listed in `THIRD-PARTY-LICENSES/README.md`) stay under
PolyForm Noncommercial 1.0.0, and the
`psxrecomp` and `recomp-ui` submodules carry their own licenses; release zips
carry their notices in `licenses/` and `assets/`. R4: Ridge Racer Type 4 is
© Namco (Bandai Namco Entertainment). Neither this repository nor its release
zips contain the disc image or the game's data (models, textures, audio,
movies, the `R4.BIN` overlays). Two things in them do come from the game:

- `generated/` is the boot EXE's code translated to C by psxrecomp, and the
  compiled game is built from it. psxrecomp keeps each original MIPS
  instruction word beside its translation (in a comment and as an argument
  to the PGXP hooks), so the EXE's code section can be read back out of
  `generated/`; its data section is not there.
- The launcher's box art (`recomp/launcher/boxart.*`, shipped as
  `assets/img/boxart.tga`) is the game's cover; see
  `THIRD-PARTY-LICENSES/README.md`.

The compiled game needs your own disc to run.

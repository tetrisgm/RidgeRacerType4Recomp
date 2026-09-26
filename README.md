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
glue. It does **not** contain the disc image, a retail BIOS, generated game C,
or decompiled game code. Builds use the MIT-licensed OpenBIOS from PCSX-Redux;
bring your own legally obtained disc.

Important files:

- `game.toml`: identity, disc digests, recompiler/runtime/video/controller/netplay config.
- `seeds/`, `annotations/`, `symbols.toml`: recompiler inputs grown from RE work.
- `tools/regen.sh`: regenerate OpenBIOS + game C from the disc.
- `tools/run_r4.sh`, `tools/dbg.py`, `tools/pad.py`, `tools/smoke.py`: run and drive a debug build.
- `renderer/adaptive/`: the MMX6 adaptive widescreen renderer, carried for later (not built by default).
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
| Widescreen | Not yet — adaptive renderer carried in `renderer/adaptive/` |

## Building From Source (macOS, Windows)

Requirements: Xcode command-line tools, `brew install cmake ninja python`,
and R4: Ridge Racer Type 4 (USA, SLUS-00797) as the Redump bin/cue (verify
against `DISC.md`). Do not convert it to a 2048-byte `.iso`: that drops the
Mode-2 Form-2 XA sectors the music and movies stream from. Linux and Windows
follow `psxrecomp/docs/BUILDING.md`.

```sh
git clone --recurse-submodules <this repo> && cd ridgeracertype4
mkdir -p disc   # put (or symlink) the .cue and .bin here
tools/regen.sh --disc "disc/R4 - Ridge Racer Type 4 (USA).cue"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DPSX_DEBUG_TOOLS=ON
cmake --build build --target psx-runtime
tools/run_r4.sh build       # straight into the game; or build/r4-runtime for the launcher
```

Code overlays (R4.BIN menus) compile to native shards in the background only
when `PSX_OVERLAY_AUTOCOMPILE_CMD` is set; `tools/run_r4.sh` / `run_r4.cmd`
set it for dev runs. Launching `build/r4-runtime` directly works, but those
menus stay in the interpreter. Release builds made by the setup host use the
`overlay_toolchain/` it installs instead, which is why `game.toml` does not
carry a dev compile command.

**Windows** builds the same way from an MSYS2 **MINGW64** shell
(`pacman -S mingw-w64-x86_64-{gcc,cmake,ninja,python} git`): run the same
`tools/regen.sh` and `cmake` commands, then start `build\r4-runtime.exe`, or
`tools\run_r4.cmd`, which also puts MinGW64 on PATH so background overlay
compiles can find `python3` and `gcc`. The exe imports only Windows system DLLs.

`tools/regen.sh` builds the recompiler into `psxrecomp/recompiler/build/` on first use,
verifies the disc against `game.toml [prepare_disc]`, extracts the boot EXE to
`disc/`, and writes `generated/`. Re-run it after changing seeds, annotations,
recompiler config, or the `psxrecomp` submodule. Drop `-DPSX_DEBUG_TOOLS=ON`
for a build without the TCP debug server. The first configure needs network
access: libjuice (netplay) is fetched, and on macOS the pinned static SDL3.

## Configuration

Most options are in the launcher and persist to `settings.toml` beside the
executable. Defaults live in `game.toml`:

- `[video]` — `renderer` (`opengl` / `software`), `aspect_ratio = "4:3"`.
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
  as on a PS1. Where the renderer cannot draw in-between frames at all, the
  package falls back to Frame blend and says so in the log.
- **Frame blend**: crossfades finished frames (cheaper, ghosts, one frame
  late).

It needs the OpenGL renderer and turns vsync off. A monitor shows at most its
own refresh rate, so rates above it cost more without showing more motion
(with vsync off they can show as tearing instead).
If the machine cannot draw every in-between frame, fewer are drawn and
crossfaded; the game never slows down. Netplay sessions run without mods.
Details and credits:
`mods/preloaded/packages/r4.enhancement.frame-rate/1.0.0/README.txt`,
`src/mods/r4_interp.c`, `psxrecomp/docs/RENDER_PASSES.md`.

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
- Framework fixes go to `psxrecomp`, not here. Resolve dispatch misses first.
- Disc images, generated code, memory cards, Ghidra databases and build outputs
  stay local. See `CLAUDE.md`.

## License

MIT for this repository's own code — see `LICENSE`. Files adapted from
MegaManX6Recomp (listed in `THIRD-PARTY-LICENSES/README.md`, including
`renderer/adaptive/`) stay under PolyForm Noncommercial 1.0.0, and the
`psxrecomp` and `recomp-ui` submodules carry their own licenses. R4: Ridge Racer Type 4 is © Namco (Bandai Namco
Entertainment); this repository contains none of the game's binaries or assets.

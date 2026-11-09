# RidgeRacerType4Recomp

R4: Ridge Racer Type 4 (USA, **SLUS-00797**), statically recompiled to a
native runtime using [psxrecomp](https://github.com/RetroPortingToolKit/psxrecomp)
and [our custom menu](https://github.com/tetrisgm/recomp-launcher).
This is an in-development preview, not a finished port.

## Current status

`master` is the single game development line. It contains the owner-reviewed
recovery and the rescued Max Detail / rewind XA fixes. Races retain the
original cadence; the old fork's packet interpolation and hold-to-rewind
implementation are not included.

The custom R4 menu opens at startup and over the game with **Select/Back**
(or Esc/Guide). Options includes display settings, Modern/Classic controls,
keyboard/gamepad bindings, input devices, deadzones, mods and volume. Save/Load
State is on the main menu. Live settings apply immediately; restart-only changes
are saved and labelled without restarting the race when you resume. PGXP stays on.

The engine retains the upstream-based recovery and Steam Controller fixes.
Always use the recorded submodule pins; see [upstream review](docs/UPSTREAM.md).
Mac verification does not qualify Windows hardware, all tracks, netplay or the
owner's reported crowded-race slowdown.

Max Detail requires expanded 8 MB RAM to prevent primitive heap corruption.
The owner's rank-8 crash route still needs manual normal-paced validation.
Preserve the owner's `present_thread = false` setting pending investigation
of temporal reversal, and retain their 4K/Max Detail quality settings.
See [current work and blockers](docs/HANDOFF.md) and
[recovery evidence](docs/recovery/README.md).

## Build on the Mac

Install Xcode command-line tools and Homebrew `cmake`, `ninja`, and Python
3.11 or newer. Supply your own USA BIN/CUE disc; verify it against [DISC.md](DISC.md).
No game data or retail BIOS is required to compile the committed native C.
The bundled OpenBIOS is open source.

```sh
git clone --branch master https://github.com/tetrisgm/RidgeRacerType4Recomp.git ridgeracertype4
cd ridgeracertype4
git submodule update --init --recursive
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DR4_BUILD_TESTS=ON -DR4_PGXP=ON -DPSX_DEBUG_TOOLS=ON \
  -DPSX_EXECUTION_PROFILE=ENHANCED -DPSX_RUNTIME_IPO=OFF
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

Tests that inspect disc sites require the USA disc in `disc/`; CMake warns
when it is absent. Select your disc in `build/r4-runtime`, or place the
BIN/CUE at `disc/R4 - Ridge Racer Type 4 (USA).{bin,cue}` and use the helper.
The nine distinct code-overlay images are compiled into the executable from
committed generated C; no runtime compiler or cache preparation is needed.
See [native overlay evidence](aot/README.md).

Automated verification uses an isolated runtime/profile and never copies
player settings or saves back:

```sh
python3 tools/verify_native_overlays.py analysis/native-check-unique-name
```

An ordinary clone/build needs no regeneration. Only after seed, annotation,
recompiler configuration or engine pin changes, run `tools/regen.sh` and
commit its generated output with the change. Never edit `generated/` by hand.

## Windows builds and delivery

All development stays on the Mac. Follow [WINDOWS_BUILD.md](docs/WINDOWS_BUILD.md)
for cross-compilation status, the build command and delivery checks.
STEAMBOX is only a gaming console; the owner tests completed portable builds
by hand. Never install Git, SDKs, compilers or agents there or connect to it.
Builds stay in new checksummed `dist/windows/<version>/` folders on the Mac. No public
release, installer or system configuration change is part of this workflow.

## References

- [Workflow](CLAUDE.md), [pin history](docs/framework_pin_history.md),
  [upstream PRs](docs/UPSTREAM.md).
- [Consolidation decisions and recovery](docs/CONSOLIDATION.md).
- [Historical fork README](docs/archive/README-before-consolidation-20261108.md)
  records the old feature set; it is not a current feature claim.
- [Widescreen](docs/WIDESCREEN.md), [disc identity](DISC.md),
  [performance investigation](docs/recovery/PERFORMANCE_HANDOFF_20261105.md).

## License

MIT for this repository's own code — see `LICENSE`. Files adapted from
MegaManX6Recomp (listed in `THIRD-PARTY-LICENSES/README.md`) stay under
PolyForm Noncommercial 1.0.0, and the
`psxrecomp` and `recomp-ui` (recomp-launcher) submodules carry their own licenses; release zips
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

The bundled HD HUD pack is Kuid0us's artwork of the HUD (see
`docs/HD_HUD.md`).

The compiled game needs your own disc to run.

### Rewind

The current recovery uses upstream's filmstrip selector with HD residency
preserved across restore. It does not use the old fork's hold-to-rewind pipeline.
Rewind is unavailable in netplay. Historical phase-5 reports describe the old
architecture and are not current instructions.

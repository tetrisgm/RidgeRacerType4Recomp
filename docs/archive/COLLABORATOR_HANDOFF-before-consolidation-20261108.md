# Rebuild the reviewed R4 recovery branch

This is a handoff for the collaborator and their coding assistant. Reproduce
this branch first, then make further changes in separate topics.

## What to check out

- Game: [`tetrisgm/RidgeRacerType4Recomp`, `recovery/upstream-20261102`](https://github.com/tetrisgm/RidgeRacerType4Recomp/tree/recovery/upstream-20261102).
- Engine submodule: `RetroPortingToolKit/psxrecomp` at **`c1f5cafd4102daa57b7342bec7c432aaca8981ce`**.
- Shared UI submodule: `RetroPortingToolKit/recomp-ui` at **`817c2985d371f8d19d70d9d1d9629425ed6cfd68`**.
- The reviewed runtime source is game commit `8a64c3f` and its ancestors.
  Later handoff/profile commits on this branch do not change that runtime code
  or either submodule pin. Record the checkout's complete HEAD when rebuilding.

The owner reviewed the recovered game on Windows and said the **4K internal
resolution with 8x anisotropic filtering** looked good enough to publish these
branches. The review is not a claim of locked 60 FPS, all-track coverage,
macOS/Linux qualification or a finished release. Race logic/presentation keeps
the original game's cadence; this branch does not include the fork's optional
packet interpolation/draw-context pipeline.

The engine performance experiments are published in the fork as **separate
parked branches**. They are not dependencies of the game branch. See
[the performance assessment](https://github.com/tetrisgm/psxrecomp/blob/review/r4-performance-candidates-20261102/docs/R4_PERFORMANCE_CANDIDATES.md).

## Instructions for the receiving assistant

Use a fresh sibling checkout and build directory. Preserve the old checkout,
native memory cards, controller bindings and settings. Read `CLAUDE.md`'s
recovery override and `docs/recovery/README.md` before the historical handoff
notes; the old README feature tables and `docs/HANDOFF.md` describe a different
fork architecture. Do not use them to reintroduce the old engine/launcher pins.

Follow the submodule commits recorded by this game branch. **Do not run
`git submodule update --remote`, merge a newer engine, or cherry-pick the
performance topics while reproducing this result.** Newer upstream work can
be considered afterward as a separately validated change.

No disc, private settings, memory cards, game executable, screenshots or local
save states were uploaded with this handoff. Obtain the disc locally from its
owner. OpenBIOS and the committed recompiled sources are already provided.

## 1. Tools

Use Git, Python **3.11 or later** with `tomllib`, CMake 3.20+, Ninja, and a
native C/C++ toolchain. The recompiler needs C++20. Keep builds at **two compile
jobs** initially. Initial dependency downloads and the large generated C
translation units can take a while.

On **Windows**, use an MSYS2 **MINGW64** shell, not the plain MSYS shell.
The tested recovery used MinGW GCC 15.2. Install missing prerequisites there:

```bash
pacman -S --needed git mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake \
  mingw-w64-x86_64-ninja mingw-w64-x86_64-python
```

On **macOS**, use Xcode command-line tools and Homebrew CMake, Ninja and Python:

```bash
brew install cmake ninja python
```

The following checkout/build commands work in either shell. This particular
recovery has not been rebuilt or visually checked on macOS yet. Linux users
should first follow the native dependency instructions in
`psxrecomp/docs/BUILDING.md`; the same source/build sequence then applies.

## 2. Fresh checkout and pinned dependencies

```bash
git clone --branch recovery/upstream-20261102 --recurse-submodules \
  https://github.com/tetrisgm/RidgeRacerType4Recomp.git R4-recovery
cd R4-recovery
git submodule sync --recursive
git submodule update --init --recursive
git status --short
git rev-parse HEAD
git -C psxrecomp rev-parse HEAD
git -C recomp-ui rev-parse HEAD
```

The last two hashes must match the engine and UI hashes above. A fresh
checkout should be clean. A `+` before a line in `git submodule status` means
that submodule differs from the recorded pin; resolve that before building.
Do not reuse an old CMake cache that points at another engine checkout.

## 3. Supply and verify the USA disc

Create `disc/` and put the matching CUE and BIN there, using these names:

```text
disc/R4 - Ridge Racer Type 4 (USA).cue
disc/R4 - Ridge Racer Type 4 (USA).bin
```

The CUE's `FILE` entry must point at the BIN. Use the USA **SLUS-00797**
single-track MODE2/2352 dump. The BIN is 621,421,920 bytes, SHA-1
`75F0DAB5C1F71BA80F3D1E2FEFD043F3F2B9EE2E`. Keep the raw BIN/CUE format;
cooked ISO conversion loses the XA audio/movie sectors. Full identity is in
[DISC.md](../../DISC.md).

```bash
python3 psxrecomp/psxrecomp_cli.py verify-disc --config game.toml \
  --project-root . --disc "disc/R4 - Ridge Racer Type 4 (USA).cue"
```

Stop on an identity mismatch and obtain the correct local image. Do not skip
the hash check or modify the identity requirements to accept another region.

## 4. Build emitters and the game

`generated/` is committed and was regenerated against this exact engine pin.
An ordinary rebuild needs no regeneration step. Build the emitters too, so
the existing development launcher can compile captured code overlays.

```bash
export CMAKE_BUILD_PARALLEL_LEVEL=2
bash psxrecomp/tools/ci/build_emitters.sh \
  --framework psxrecomp --build-dir build-recompiler --jobs 2

cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DR4_BUILD_TESTS=ON -DR4_PGXP=ON \
  -DPSX_RECOMP_UI=ON -DPSX_DEBUG_TOOLS=ON \
  -DPSX_EXECUTION_PROFILE=ENHANCED -DPSX_RUNTIME_IPO=OFF

cmake --build build --parallel 2
ctest --test-dir build --output-on-failure --parallel 1
```

The executable is `build/r4-runtime.exe` on Windows or `build/r4-runtime`
on macOS/Linux. Build all targets before CTest, since some tests have their own
executables. The verified Windows build passed **26/26** tests with the local
disc present. Tests requiring the disc must not silently disappear from a
supposedly equivalent qualification. Use `ctest --test-dir build -N` to inspect
what was registered.

If a compiler was not found, fix the shell/toolchain. If an asset or dependency
is missing, repair the recursive submodules or dependency download. Do not
disable native CPU spans, PGXP, emitter checks or required tests to get a green
build. If CMake reports stale generated sources after an intentional codegen
input or engine change, use the supported regeneration path, then reconfigure
and rebuild:

```bash
JOBS=2 bash tools/regen.sh --disc "disc/R4 - Ridge Racer Type 4 (USA).cue"
```

Never hand-edit `generated/*.c`. Save states from a different build are
disposable and can be rejected; native memory cards are player data and must
be preserved.

## 5. Reproduce the approved 4K/8x picture

Apply the checked-in profile **after building**:

```bash
python3 tools/apply_recovery_4k.py --runtime-dir build
bash tools/run_r4.sh build
```

The existing Windows-native alternative is:

```text
tools\run_r4.cmd build --no-launcher
```

The profile is [approved-4k.toml](approved-4k.toml). It selects 4K, 8x
anisotropic mipmapping, xBR texture filtering, FXAA HQ, bicubic FMVs, enhanced
audio, full PGXP, maximum course/car detail and draw distance, full mirror
scenery, the visible mirror, and HD HUD artwork. Widescreen remains enabled
at adaptive **Fit to Window**, with Car Effects, camera and Modern controls
retained from the branch defaults. The separate modern Race HUD stays at its
default; this preview uses the HD version of the game's own HUD.

The helper edits only the selected built runtime's `game.toml` and graphics
feature selections in `mods/state.toml`. It backs those files up as
`*.before-recovery-4k`, preserves unrelated mod choices and custom pack paths,
and leaves `settings.toml`, controller bindings and native saves intact.
Run it again after a build regenerates `build/game.toml`. A previously saved
window mode may override the template; choose Windowed in the launcher if
reusing an existing profile. A fresh build directory avoids stale choices.

Clear stale `PSX_*` graphics/debug overrides and `SDL_AUDIODRIVER=dummy` from
the launching shell if a prior test left them set. The owner-facing playtest
should be a normal window with normal sound. Automated tests stay hidden and
muted. Modern controls use RT for gas, LT for brake and R1 for the camera.

Do not use 8K to reproduce the accepted setup. A 4K height preset is **9x**
the 240-line race frame; **8x here means anisotropic filtering**, not an
8x render scale. Hardware limits may lower the effective rendering scale.
To check the running development build:

```bash
python3 tools/dbg.py --port 4797 video_info
python3 tools/dbg.py --port 4797 texture_lod
```

On the reviewed Windows host, those report preset `2160`, effective scale
`9`, dynamic resolution `0`, and texture LOD mode `1` / anisotropy `8`.
Do not promise the same performance on a different GPU merely because the
requested setting matches.

## 6. Validate before changing the implementation

Reach a real race and drive through straights, bends, tunnel entrances/exits
and distant scenery. Take moving samples and inspect road/sky holes, incorrect
culling, draw-distance pops, seams, mirror geometry, the car and HUD placement.
Check normal audio and controls, then current-build save/load and rewind.
Loading a race or looking at one screenshot is not complete visual coverage.

Use the game's TCP interface for automation, preserving desktop focus. For
the actual fitted window/aspect image use `present_shot`, then poll
`present_shot_seq` for completion. Use `screenshot_hires` or
`screenshot_wide_hires` for full-resolution geometry. Plain `screenshot`
captures native VRAM and cannot prove PGXP/high-resolution output. Existing
commands and scripts are documented in `psxrecomp/docs/TCP_COMMANDS.md`,
`tools/smoke.py`, `tools/test_modern_controls_runtime.py` and
`docs/recovery/README.md`. Record actual passes, failures and unverified areas.

The earlier Windows recovery has moving Helter Skelter visual samples,
save/load, rewind, controls and measured original/recovery comparisons.
Those are useful prior evidence, not blanket qualification for a new host,
all tracks, every Car Effects trigger or netplay. New screenshots/binaries and
raw local evidence must remain outside Git, as must the disc and saves.

## Separate performance branches and the intended direction

Engine branches live in `tetrisgm/psxrecomp`, not in this game's submodule pin:

```bash
git -C psxrecomp remote add r4-fork https://github.com/tetrisgm/psxrecomp.git
git -C psxrecomp fetch r4-fork
```

Inspect them without switching the engine underneath the working game. The
six upstream-based topics are `perf/r4-render-ring-20261102`,
`perf/r4-gl-state-cache-20261102`, `perf/r4-gl-samplers-20261102`,
`perf/r4-disc-prefetch-20261102`, `perf/r4-hd-miss-path-20261102`, and
`perf/r4-render-priority-20261102`. Original draw-context, later presentation
and rewind-worker snapshots also remain on `perf/r4-*-original-20261102`
branches. Full identities, dependencies and test limits are in the linked
performance assessment. Component checks pass within their stated scopes;
gameplay performance gains and full visual correctness remain unqualified.

The visual/performance goals were reasonable. The recovery keeps their intent
while challenging the coupling of small optimizations to a much larger,
partially qualified renderer redesign on an older engine. Keep the recovered
baseline working, then replay one coherent improvement at a time. Use matched
built-game tests and meaningful visuals, and keep a measured result separate
from a profile, a unit test or a historical claim. Read
[TECHNICAL_ASSESSMENT.md](TECHNICAL_ASSESSMENT.md) and
[COLLABORATOR_ASSESSMENT.md](COLLABORATOR_ASSESSMENT.md) for the fuller,
constructive findings.

The archive branches preserve the original work. Nothing in this handoff
authorizes merging all experiments, changing the default branches, publishing
a release, adding the disabled loading-speed mods to a release, or replacing
player saves. Further work should be reviewed on its own merits.

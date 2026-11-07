# Codex brief: R4 performance on the owner's PC (2026-10-28)

You are working on **RidgeRacerType4Recomp** (static recompilation of Ridge
Racer Type 4, PS1, to native code with the psxrecomp framework) in
`C:\Users\Shokunin\dev\ridgeracertype4` on the owner's Windows PC
(DESKTOP-BAKIEMJ: Ryzen 7 9800X3D 8C/16T, RTX 4080 SUPER, 47 GB RAM,
7680x4320 60 Hz panel). Read `CLAUDE.md` and `psxrecomp/CLAUDE.md` first and
follow them.

## The problem — the only thing that matters now

The owner playtested the current build (`C:\Users\Shokunin\Games\R4-test`,
built from master e35d2ed, defaults, launched from the desktop) and says:
performance is **completely unacceptable** — the game audibly struggles,
**even the audio is choppy**. Choppy audio means the emulation/main thread is
not keeping real time (audio is produced by the guest SPU at the guest
cadence), not just slow presentation. Earlier builds on this same machine ran
races at ~59 distinct frames/s, so this is very likely a regression from
recent default/feature changes. Fix it. Nothing else.

## What changed recently (prime suspects, newest first)

All landed 2026-10-27/28 as R4 PRs #85-#93; framework on fork
`tetrisgm/psxrecomp` branch `r4/main` (pinned 2b5a3817), launcher
`tetrisgm/recomp-launcher` r4/main (pinned 5d8eaa3):

1. **Controller input** (psxrecomp `runtime/src/main.cpp` ~6119
   `refresh_player_devices`, ~6428-6470 Steam echo filter;
   `runtime/include/psx_controller_type.h`): keyboard P1 seat adopts the first
   gamepad; a filter ignores keyboard echo while a Valve pad is active. Check
   it does no per-frame device enumeration / SDL calls on the hot path.
2. **In-game menu at boot / Select tap / main screen / save states**
   (recomp-ui `src/r4l/overlay_entry.cpp`, psxrecomp `ovl_sink` now calls
   `psx_host_set_ui_capture` on every event). Check the overlay is not
   drawing/ticking while closed, and that input capture isn't toggling.
3. **PGXP CPU-mode propagation turned ON by default**
   (`mods/preloaded/packages/psx.enhancement.pgxp/1.0.0/manifest.toml`
   `cpu_mode` default "true"). This tracks precision through CPU arithmetic
   in the recompiled code — potentially a large per-instruction CPU cost.
   Very strong suspect for the emulation thread falling behind.
4. **Car Effects ON by default** (`mods/preloaded/packages/r4.enhancement.car-effects`,
   PC particles + camera smoothing + lighting values).
5. **Rewind ON by default** (`game.toml [video] rewind = true`): whole-machine
   snapshot (~3.5 MB) every `rewind_interval` (15) frames on the emulation
   thread.
6. **Race frame rate "Auto"**: 60 FPS, or 30 at internal scale >= 18
   (`src/mods/r4_frame_rate_plugin.c`). At Match display on the 8K panel the
   scale is 18x; in-between frames cost the emulation thread most of a game
   frame (README "Race frame rate").
7. **Dynamic resolution OFF** (fixed 18x at 8K = 4320 lines, GPU-heavy) and
   **borderless desktop fullscreen** by default.
8. PGXP seam-strip rewrite (fork 9d93976c), bicubic FMV filter.

PGXP itself (geometry correction + perspective texturing) must stay ON — the
owner forbids turning it off, even as a test reference. You may bisect its
sub-features (CPU mode, depth buffer, colour correction, seams). Everything
else in the list above may be toggled to bisect, and if one is the cause,
fix it properly; if it cannot be made cheap, turn it back off by default and
say so.

## How to measure (do this first, then bisect)

- Owner rule: **never open a visible game window or change the display mode
  while the owner may be at the PC**. Use `--hidden-window` (real GL, no
  visible window) or `--headless`. Kill only processes you started (stop the
  `r4-runtime.exe` PID itself; killing a `tools/run_r4.sh` shell leaves the
  game running). If the owner closes something, leave it closed.
- Dev build: `build/` (configured with -DPSX_DEBUG_TOOLS=ON
  -DR4_BUILD_TESTS=ON). Build in Git Bash with
  `PATH="/c/msys64/mingw64/bin:/c/msys64/usr/bin:$PATH"`, `ninja -C build`
  (retry once on a Ninja "()" version error). For Python tools use 3.12:
  `C:\Users\Shokunin\AppData\Local\Programs\Python\Python312\python.exe`
  (MSYS python3 fails: export USERPROFILE='C:\Users\Shokunin' and
  HOME=/c/Users/Shokunin if you use it).
- Start: `tools/run_r4.sh build --hidden-window --debug-port 4797`
  (`--no-launcher` is added by the script). `tools/smoke.py OUT_DIR` drives
  boot -> race over the debug server (turbo; dispatch misses must be 0).
  `tools/test_modern_controls_runtime.py grid --slot 9` makes a Grand Prix
  grid savestate; `tools/dbg.py` has the debug-server client (savestate load,
  counters). Measure in a race at REAL speed (turbo off): emulation-thread
  frame time / VBlank lateness, audio underruns (look for SPU/audio
  underrun counters or add one), render-thread and present counters
  (`PSX_GL_ASYNC_GPU_TIMER=1`, `PSX_GL_ASYNC_GPU_OPS`), CPU per thread
  (Windows perf counters / `psxrecomp/tools/host_profile*` with
  `--nm C:/msys64/mingw64/bin/nm.exe`).
- Match the owner: the release build in `C:\Users\Shokunin\Games\R4-test`
  runs Match display = 18x on this 8K panel. The dev build must use the same
  settings (shipped `game.toml` defaults; `build/settings.toml` can hold
  stale values — check it).
- Bisect fast: one variable per run, same savestate, 20-30 s each. Report a
  table: config -> emulation frames/s at real speed, max frame time, audio
  underruns, distinct presents/s.

## Rules for changes

- Framework (engine/runtime) fixes go to the fork: branch `fix/<name>` off
  `tetrisgm/psxrecomp` r4/main in `psxrecomp/` (remote `fork` =
  https://github.com/tetrisgm/psxrecomp), merge into r4/main, push. Then pin
  it in R4 on a branch, PR, merge (we merge our own R4 PRs:
  `gh pr create`/`gh pr merge --merge`).
- Never edit `generated/` by hand. Editing anything under
  `psxrecomp/recompiler/` stales the OpenBIOS stamp: then run
  `psxrecomp/tools/regen_bios.sh --config bios/OpenBIOS.toml` (build the
  emitter first: `cmake --build psxrecomp/recompiler/build --target
  psxrecomp-bios`), check with `psxrecomp/tools/ci/check_bios_stamps.sh`,
  commit `generated/OpenBIOS*` in the fork.
- Commit identity: `git -c user.name=Shokunin -c
  user.email=30949000+tetrisgm@users.noreply.github.com commit ...`. Stage
  paths explicitly (never `git commit -a`: the submodule gitlink sneaks in).
- Before merging: `ctest --test-dir build` 32/32, one `tools/smoke.py` run
  with 0 dispatch misses, the measurement table showing the fix.
- Record the pin in `docs/framework_pin_history.md`, the result in
  `docs/VERIFIED.md`, current state in `docs/HANDOFF.md`.
- Then rebuild the playtest: `tools/package_release.sh HEAD` from an MSYS2
  MINGW64 shell with Git on PATH, reinstall `C:\Users\Shokunin\Games\R4-test`
  keeping `input.ini keybinds.ini disc.cfg bios.cfg rom.cfg saves\` and
  dropping `settings.toml`. Do not launch it; the owner tests.

## Done means

The owner's default configuration on this PC runs races with the emulation
thread at real time (no audio choppiness, no missed VBlanks) and smooth
presentation; you give the owner a short table of before/after numbers and
the one or two causes you found.

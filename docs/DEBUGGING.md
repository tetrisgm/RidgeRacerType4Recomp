# Debugging R4 — orientation for a new collaborator

Written 2026-11-02 for mstan. Everything here is checked against the tree as
of R4 master `14d7981` / psxrecomp fork `84d6ca22`. Issue
[#106](https://github.com/tetrisgm/RidgeRacerType4Recomp/issues/106) is the
live tracker for the gameplay artifacts; this file is the map.

## 1. What this is

R4 is a static recompilation of *R4: Ridge Racer Type 4 (USA)*, SLUS-00797,
on the shared **psxrecomp** framework. The game's MIPS code is translated to C
once (`generated/`, committed) and compiled into `r4-runtime.exe` together with
the framework runtime (GPU/SPU/CD emulation in native code, no interpreter on
the hot path). Releases are compiled from this repo as is; no emulator sits
behind the binary.

Three repositories, each pinned by the one above it:

| Repo | What | Branch we build | Remote |
|---|---|---|---|
| `tetrisgm/RidgeRacerType4Recomp` (this) | the game: config, seeds, mods, launcher title layer, generated C | `master` | github |
| `tetrisgm/psxrecomp` (fork of `RetroPortingToolKit/psxrecomp`) | framework: recompiler + runtime | `r4/main` = upstream master + our topic branches | submodule `psxrecomp/` |
| `RetroPortingToolKit/recomp-launcher` | in-game menu / launcher UI (ImGui) | `r4/main` | submodule `recomp-ui/` |

Fork rule (CLAUDE.md): framework work lands on a `fix/<name>` or
`feat/<name>` topic branch off upstream master, is merged into `r4/main`, then
R4 bumps its submodule pin. `docs/UPSTREAM.md` lists every fork delta;
`docs/framework_pin_history.md` says why each pin moved. Nothing in
`generated/` is ever edited by hand — fix the framework or `game.toml` and
regenerate (`tools/regen.sh`).

## 2. Where things live

```
game.toml                 all per-title config: [video], [quality.*], mods, widescreen cull lists
generated/                recompiled game C (committed); psxrecomp/generated/ holds OpenBIOS C
launcher/r4_menu.cpp      R4 in-game settings using the shared UI and menu-host contract
src/mods/                 R4's trusted plugins (C/C++): widescreen renderer, frame-rate,
                          car effects, max-detail, race HUD, modern controls, camera look
mods/preloaded/packages/  the mod manifests (options, defaults) shipped with the game
tools/                    smoke.py, dbg.py, pad.py, perf/, regen.sh, package_release.sh
docs/                     FRAME_ARCHITECTURE.md, FRAME_PIPELINE.md, WIDESCREEN.md, VERIFIED.md ...
psxrecomp/runtime/src/    the runtime: main.cpp (host, settings, menu host),
                          gpu_gl_renderer.c (GL backend), frame_owner.cpp (render thread),
                          packet_presenter.cpp / packet_interpolation_stream.cpp (60 FPS),
                          mod_runtime.cpp (mods), debug_server.c (TCP debug ops)
psxrecomp/docs/           TCP_COMMANDS.md, RENDER_THREAD.md, ENHANCEMENTS.md, QUALITY_PRESETS.md
recomp-ui/src/r4l/        the menu: ui/screens_settings.cpp (Graphics page), core/surface.cpp
                          (row keys), psx_menu_provider.cpp (adapter to the runtime)
```

Player-side state next to the exe: `settings.toml` ([video] overrides that
outrank `game.toml`), `mods/state.toml` (feature on/off and option values),
`saves/`, `input.ini`, `keybinds.ini`.

## 3. Build and test (Windows; Mac is the same with brew cmake/ninja)

```bash
git clone --recursive https://github.com/tetrisgm/RidgeRacerType4Recomp
cd RidgeRacerType4Recomp
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DPSX_DEBUG_TOOLS=ON
cmake --build build --target psx-runtime      # produces build/r4-runtime.exe
ctest --test-dir build                        # R4 tests (25)
```

`-DPSX_DEBUG_TOOLS=ON` compiles the TCP debug server (port 4797) and the live
A/B keys; every script in `tools/` needs it. The disc is not in the repo:
put your dump at `disc/R4 - Ridge Racer Type 4 (USA).cue` (hashes in
`DISC.md`) or pass `--disc`.

Framework unit tests (separate CMake tree, no UI):

```bash
cmake -S psxrecomp/runtime -B psxrecomp/build-tests -G Ninja -DPSX_RECOMP_UI=OFF
cmake --build psxrecomp/build-tests && (cd psxrecomp/build-tests && ctest)
```

Gotchas:

- `psxrecomp/generated/OpenBIOS.emitter.sha` is a fingerprint over the
  recompiler sources listed in `psxrecomp/tools/bios_emitter_fingerprint.cmake`
  (this includes `recompiler/src/config_loader.cpp`). Touch any of them and
  configure fails "STALE" until you run
  `psxrecomp/tools/regen_bios.sh --config bios/OpenBIOS.toml` and commit the
  new stamp (the C is usually byte-identical).
- Same rule for the game: after a seed/annotation/`[recompiler]` change run
  `tools/regen.sh` and commit `generated/` in the same change.
- The Windows build uses the clang toolchain the launcher installs
  (`%LOCALAPPDATA%\retcomm\toolchains\cmake-clang-v1`); test executables need
  its `bin` on `PATH` for the DLLs.

## 4. Running and driving the game without touching the screen

```bash
tools/run_r4.sh build --hidden-window          # SDL_AUDIODRIVER=dummy is a good idea too
python tools/dbg.py ping                       # any op from psxrecomp/docs/TCP_COMMANDS.md
python tools/smoke.py out/ --port 4797         # boot -> race in seconds (frontend turbo)
python tools/dbg.py savestate op=load slot=0   # slots in saves/; slot 0 = mid-race, moving
python tools/dbg.py set_input ...              # pad input (see tools/pad.py)
```

Useful debug ops for the render path: `render_thread` (frames produced /
consumed / **replaced**, `packet_cpu` with `phase_ns` seed/restage/replay/
present/flush, `tri_ns`, `prep_ns`), `packet_interpolation` (presenter:
interval, late, resyncs), `video_set` (internal resolution, supersample in
thousandths 1000..4000), `pgxp`, `bloom`, `texture_lod`, `dithering`,
`debug_key` (the A/B keys: 7 geometry correction, 8 perspective texturing,
9 PGXP depth, F10 PGXP colour, F11 seam, F12 CPU mode, 0 summary).
`PSX_GL_ASYNC_GPU_TIMER=1` adds GPU time per packet; `PSX_RENDER_OP_PROFILE=1`
gives per-op cost (op N in the dump = enum index N-1 in gpu_gl_renderer.c;
11 textured tri, 12 shaded textured tri, 16 line).

`tools/perf/`:

- `measure.py [label] [RATE]` — loads slot 0, drives 14 s with the autopilot,
  prints per-packet render-thread cost (phases, tri, prep, GPU) and the
  replaced-packet ratio. The first thing to run for any "it stutters" report.
- `autopilot.py N` — steers from the track centreline in RAM so the car does
  not hit walls (artifacts need motion to show).
- `opsprof.py` — per-op profile (needs `PSX_RENDER_OP_PROFILE=1`).
- `d.py` — thin wrapper; `R4_DEBUG_PORT` overrides 4797.

On the Mac, `sample <pid>` gives render-thread stacks (`FrameOwner::run`);
Windows has no sampler for it, use the `packet_cpu` timers.

## 5. How a frame gets to the screen (the part that is breaking)

R4 runs its race logic at 30 fps. The framework's frame architecture
(`docs/FRAME_ARCHITECTURE.md`, `docs/FRAME_PIPELINE.md`):

1. The guest thread publishes one **packet** per VBlank (the GP0 command
   stream plus native VRAM input tiles and PGXP provenance) into a
   `PsxFrameMailbox`: latest-complete-frame, so an unclaimed pending packet is
   **replaced** (`frames_replaced`).
2. The `FrameOwner` render thread takes packets: `rth_packet_execute` →
   `rth_packet_prepare` (collects the interpolation stream) →
   `packet_presenter_submit` → `rth_packet_draw` (seed, restage, replay the
   commands in HD, present, flush).
3. The "60 FPS" option is the `r4.enhancement.frame-rate` mod: it sets a
   packet-interpolation rate and the presenter (`packet_presenter.cpp`) draws
   in-between frames by interpolating geometry between two packets. Guest
   timing is untouched; this is presentation only.
4. A **gap** in packet sequence (the consumer missed one) used to do two
   things: `gl_renderer_restage_vram_after_savestate()` dropped every HD
   region-pool surface and re-uploaded all of VRAM (the low-res flash), and the
   presenter interpolated across the gap (invented motion).

So everything the owner sees — blinking polygons, textures dropping to
low-res, objects popping, walls sliding — is the render thread not keeping up
with the guest, and the gap handling making the misses visible. The guest
itself is fine: the HUD lap timer in the recordings jumps by several frames
exactly where the artifacts are.

### What was measured (owner's PC, RTX-class GPU, 4K display, 1.5x supersample)

Before `84d6ca22`: ~25% of packets replaced; per packet ~11 ms, of which
~10 ms was `pack_flush` re-uploading the *bounding box* of all native input
tiles (~1.4 MB) through the 1 MB `buffer_stream_upload` ring with fence waits.

After: replay ~2 ms (dirty 64×64 tiles only), present wait 6–7 ms (GPU-bound
at that resolution), interpolation prepare ~5 ms (3.5 ms matching primitives
to their history by identity, 1 ms pairing; `packet_interpolation_stream.cpp`
`stream_end_impl`), ~10% replaced.

Mac (M-series, Apple GL): draw-call bound, ~9 ms per batched draw, ~28%
replaced, unchanged by the fix above.

### Open, in order of value

1. **Make a dropped packet invisible.** On a gap, re-upload only the tiles
   changed since the last consumed packet and keep the HD surfaces
   (dirty-tile tracking already exists in `pack_flush` /
   `rth_packet_native_input`, `s_pack_dirty_tiles`). Then a drop is one
   repeated frame, nothing on screen breaks.
2. **Prepare pass off the render thread / cheaper.** Identity matching is a
   hash job, not a scan; and prepare can overlap the GPU. Target: render
   thread under 12 ms at 4K so replaced ≈ 0.
3. **Seams** between track polygons: not bisected yet. Suspect the
   interpolated frames (compare with the frame-rate mod at 30 FPS) and the
   PGXP seam mode (`pgxp_seam` fine/wide/off; F11).
4. Occasional ~25 ms guest-side spike (not render thread).
5. Mac draw-call cost (Apple GL batching or Metal) — big, lower priority; the
   Mac is a test bench, the PC is the target.

A quick control: set the frame-rate mod to "30 FPS (original)" in the menu or
`mods/state.toml`. Prepare cost and all in-between frames go away, replaced
packets go to ~0 and no artifact appears. That isolates the 60 FPS path.

## 6. Settings and switches you will want

- PGXP is on by default for R4 via the `psx.enhancement.pgxp` mod with
  `pgxp_mod_only = true` in `game.toml` (the mod is the one switch). Its
  renderer sub-features are `[video]` keys: `pgxp_depth_buffer`,
  `pgxp_color_correction`, `pgxp_seam`. Project rule: PGXP stays on in
  anything we ship; bisect its sub-features rather than turning it off.
- Rewind (`rewind = true`), 60 FPS, car effects, widescreen (Fit), HD HUD
  (T4HDHUD by Kuid0us) are all on by default. Enhancements are elective;
  correctness fixes are default-on, never opt-in.
- `settings.toml` beside the exe overrides `game.toml [video]`; the in-game
  menu writes it. Quality presets (`[quality.low..ultra]`) govern some keys;
  a live edit of a governed key makes the preset "Custom".
- Never run the owner's installed copy (`C:\Users\Shokunin\Games\R4-test`)
  from automation while they may be at the PC; use `--hidden-window`.

## 7. Work in flight (branches, not merged)

- Live Graphics menu, owner request 2026-11-02 ("all graphics options in the
  main menu, on/off each, even PGXP, applied instantly"):
  psxrecomp `feat/live-image-settings` (`a4a5a7d4`: bloom / texture LOD /
  anisotropy / dithering / PGXP depth-colour-seam as live
  `PsxHostVideoSettings` rows saved to settings.toml; a generic
  `psx_mod_register_feature_toggled_plugin` live feature switch; PGXP mod flips
  live), recomp-ui `feat/live-image-settings` (`6bca215`: the rows), R4
  `feat/live-graphics-menu` (`6c1a040`: title manifest shows every row,
  frame-rate and car-effects plugins register live callbacks). Builds; not yet
  verified on screen; the framework unit-test tree showed a compile failure to
  look at (`psxrecomp/build-tests`).
- Everything older is in `docs/HANDOFF.md` (long, chronological) and
  `docs/VERIFIED.md` (what has been checked on screen and when).

## 8. Conventions

Commits: short imperative subject, body says why, trailer
`Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>` when Claude wrote it;
author is Shokunin. PRs link their sibling PRs in the other repos. Small PRs,
evidence in the body (numbers, test names, screenshots). Public credit is
"Shokunin" / RetroPortingToolkit.

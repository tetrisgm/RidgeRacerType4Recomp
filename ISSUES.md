# RidgeRacerType4Recomp — Issues

Current state (bring-up, 2026-09-25): boots from the recompiled OpenBIOS (both
the HLE boot-skip and the full LLE intro), plays the intro movie with XA audio,
reaches the menus, loads and runs Grand Prix races, and runs 2-player VS Battle
over netplay (delay-sync and rollback) with matching state digests on both
peers. 0 dispatch misses in every run so far.

---

## #1 — Full playthrough not yet verified — OPEN

Verified: intro/attract, main menu, Grand Prix setup screens, race start and
driving, VS Battle car/course select and race. Not yet verified: finishing a
full Grand Prix season, Time Attack, Extra Trial, Garage/save-load to a memory
card, Records, the ending movie. Report hangs or wrong behaviour with the mode
and what you did.

## #2 — Link battle (SIO1 link cable) unsupported — OPEN (framework)

R4 registers the libcomb "sio" device at every boot and its link-battle overlay
(R4.BIN entry 667) drives SIO1. psxrecomp has no SIO1 model
(`runtime/src/memory.c` only charges timing), so link battle ends in
"LINK ERROR!". Netplay VS Battle (split screen) is the supported 2-player path.

## #3 — NeGcon / JogCon not modeled — OPEN (framework)

The game identifies digital (0x41), DualShock (0x73), mouse (0x12), NeGcon
(0x23) and JogCon (0xE3) pads. The framework presents digital or DualShock
only, and netplay carries buttons + 4 axes. Analog steering therefore comes
from DualShock mode, not NeGcon twist.

## #4 — First visit to an overlay mode runs interpreted — OPEN (enhancement)

R4.BIN entries 659–672 are code overlays linked at 0x801149A8 (garage,
VS car select, course info, records, link battle, movie player). The runtime
captures each on first execution and compiles a native shard in the background
(release builds: the setup host's `overlay_toolchain/`; dev runs:
`PSX_OVERLAY_AUTOCOMPILE_CMD` via `tools/run_r4.*`); until then that code runs in the
dirty-RAM interpreter. Shards persist in `<build>/cache/`. Ahead-of-time
shards need an R4.BIN extraction method (a cumulative start-sector table, which
`tools/aot_overlay_pipeline.py` does not read yet).

## #6 — Widescreen (R4 Custom Renderer) limitations — OPEN (enhancement)

The `r4.enhancement.widescreen` mod (experimental, off by default; see
`docs/WIDESCREEN.md`) widens the frames of R4's five race handlers (the
race overlays 659/660/661, the attract demo and the after-goal run). Known
limits:

- Extra Trial and link-battle races (overlays 666/667) stay 4:3 until their
  frame handlers are added to the race predicate.
- Fit to Window past about 65:9 (a short, very wide window): the GTE's
  screen-X range lets the course reach 703 px right of the 4:3 frame (1024 px
  on the left), so the rest of the right reveal renders black under the
  right-hand HUD, and past about 89:9 the left edge too. Fit stays uncapped
  by owner decision (a cap only if the primitive heap forces one); see
  `docs/WIDESCREEN.md`.
- A wall right beside the car: with the car against a wall, or nosing into
  one, the columns on that side past a vertical edge show the scenery behind
  the wall. The stock 4:3 game shows it too (edge at X = 137, 185 and 286 in
  three frames of one nose-in savestate); wide views reveal it more often.
  One cause is the GTE's perspective divide, which saturates for vertices at
  depth <= H/2 (145) or behind the camera, as Beetle's does, so the near
  part of a wall at distance d collapses onto X = 160 + 2d. That line is in
  the frame when d < 80 at 4:3, 106 at 16:9, 140 at 21:9 and 213 at 32:9;
  in a side-scrape savestate at d = 143 only 32:9 shows it (142 of 854
  columns). The nose-in band doesn't respond to the divide, so it has a
  second cause, not found yet. R4's SZ 288/290 near compares are not the
  cause: moving them changes no pixels. A fix needs near-plane clipping
  inside R4's course renderers for the divide case, and the second cause
  found; no `[widescreen.cull]` site kind can do either. An unsaturated
  wide-view divide was tried and draws wedges. See `docs/WIDESCREEN.md`.
- The rear-view mirror needs OpenGL in wide races: only OpenGL copies the
  canonical 4:3 column (where the mirror is drawn) into the wide surface. On
  the software renderer it shows solid black (checked); Vulkan was not run
  (not built on macOS) and has the same code shape. Framework follow-up U7 in
  the enhancement plan.
- Netplay VS always runs stock 4:3 (mods are cleared). Offline 2P VS widens
  correctly (checked at 16:9, 21:9 and 32:9 with `tools/pad2.py` supplying
  pad 2; the divider spans the full width).
- The mod's entry hooks belong to its manifest plugin (`r4.widescreen`):
  psxrecomp runs them only while the resolved mod plan activates it, so never
  in netplay or while the package is disabled.

## Watch items

- `psxrecomp` hard-codes four MotK PCs (`gpu.c`, `psx_netplay_rb.c`); two fall
  inside R4's hand-written renderer (0x8005F000–0x8006E000). None is an
  interrupt-check site in the current codegen. Re-check after every regen:
  `grep 'psx_check_interrupts_at(cpu, 0x\(8006CD54\|8006CDA0\|800768C8\|80076880\)u)' generated/*.c`
- Netplay peers should run the same binary with the same overlay caches; a
  lazily compiled overlay moves native/interpreted boundaries. Both peers
  stayed in sync in testing, but a peer with an empty cache is the first thing
  to suspect on a desync.
- The game rewrites part of its own text at 0x8009612C (Psy-Q libcard's
  self-disabling patch routine). The runtime interprets that one diverged page;
  this is expected.

---

## Resolved

### #5 — macOS build linked Homebrew SDL3/FreeType dylibs — ✅ FIXED
A local build linked `/opt/homebrew/…/libSDL3.0.dylib` plus Homebrew
freetype/harfbuzz and targeted the build machine's macOS, so it would not run
on another Mac. `CMakeLists.txt` now defaults macOS builds (including the one a
player's setup host makes) to psxrecomp's pinned static SDL3, no optional
FreeType/HarfBuzz and a macOS 11.0 floor; `tools/package_release.sh` gates the
release binaries on system-only libraries, universal arm64+x86_64 and minos
11.0.

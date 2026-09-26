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
(`[runtime] overlay_autocompile_cmd`); until then that code runs in the
dirty-RAM interpreter. Shards persist in `<build>/cache/`. Ahead-of-time
shards need an R4.BIN extraction method (a cumulative start-sector table, which
`tools/aot_overlay_pipeline.py` does not read yet).

## #5 — macOS build links Homebrew SDL3 dynamically — OPEN (packaging)

A local build links `/opt/homebrew/opt/sdl3/lib/libSDL3.0.dylib` (plus
Homebrew freetype/harfbuzz), so the binary is not portable to another Mac.
A distributable build should force the pinned static SDL3
(`-DCMAKE_DISABLE_FIND_PACKAGE_SDL3=ON`, untested) or use the framework's
macOS packager.

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

(none yet)

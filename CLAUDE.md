# RidgeRacerType4Recomp Rules

Static recompilation of **R4: Ridge Racer Type 4 (USA)** — serial
**SLUS-00797**, boot EXE `SLUS_007.97` — to native code, built with the shared
**psxrecomp** framework. The goal is a binary that plays without an emulator
behind it, like TombaRecomp and MegaManX6Recomp.

## Inheritance

This project inherits, in order:

1. The framework constitution at `psxrecomp/CLAUDE.md`. Read it first: no MIPS
   interpreter as the answer, no HLE BIOS shims, no stubs, recompiled BIOS
   first, fix the framework/runtime/config and **regenerate** — never hand-edit
   `generated/`.
2. The repo shape of `mstan/MegaManX6Recomp`, updated to the framework's
   New Project Layout (`psxrecomp/` + `recomp-ui/` root submodules,
   `psxrecomp_add_game_runtime`, `codegen_setup.c`).

## Project rules

- Game binaries (disc image, extracted boot EXE, Ghidra dumps), Ghidra
  databases, memory cards, overlay captures, and build outputs are **local
  only** and must not be committed. See `.gitignore`.
- Tracked: `game.toml`, `seeds/`, `annotations/`, `symbols.toml`,
  `ghidra/instructions.txt`, `ghidra/scripts/`, `CMakeLists.txt`,
  `codegen_setup.*`, `tools/`, `mods/preloaded/`, `renderer/`, docs.
- Codegen/runtime fixes belong in the framework (`psxrecomp/`) or in per-game
  `game.toml` config — never in `generated/*.c`. A fix only this game needs is
  a smell; prefer a class fix the next title inherits.
- After every run, resolve all dispatch misses before any other debugging.
- The framework and launcher versions are the `psxrecomp` and `recomp-ui`
  submodule gitlinks. Bump them deliberately; record why in
  `docs/framework_pin_history.md`.
- Base 4:3 uses the stock psxrecomp renderer. The MMX6 adaptive renderer is
  carried under `renderer/adaptive/` for later widescreen work and is not
  built by default (see `renderer/adaptive/README.md`).
- Disc identity and hashes live in `DISC.md`; verify a dump against them before
  blaming a regression.

## Feature workflow (owner rules)

- Engine/runtime/recompiler/launcher changes go to `psxrecomp` (or
  `recomp-ui`) as PRs upstream; game-specific work goes to this repo. Never
  patch the submodules in place on master.
- psxrecomp's default behaviour stays faithful for every PS1 title. Enhancements
  are elective and opt-in (off by default, enabled by config or a mod), never a
  change to the base path.
- Each feature set is developed in a worktree pair (this repo + psxrecomp, plus
  recomp-ui when touched) on matching `feat/<name>` branches, and lands as small
  PRs in every repo it touches. Each PR body says what the feature does and
  links its sibling PRs.
- A local integration branch that bundles several features for building and
  playtesting is fine, but it is never the source of truth and never merged;
  the per-repo PRs are what gets reviewed and landed.
- Widescreen and similar presentation work use a custom renderer in the
  MMX6/Tomba pattern (see `renderer/adaptive/`).

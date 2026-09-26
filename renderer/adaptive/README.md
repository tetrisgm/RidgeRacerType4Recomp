# Adaptive renderer (carried from Mega Man X6 Recompiled)

The default build uses the **stock psxrecomp renderer at 4:3**. This directory
carries the custom adaptive-widescreen renderer from
[MegaManX6Recomp](https://github.com/mstan/MegaManX6Recomp) so R4's widescreen
work can start from it. Nothing here is compiled by the default build.

## What is here

| Path | What |
|---|---|
| `BASE` | Framework commit the patch is verified against, plus provenance SHAs. |
| `framework-patches/0001-…-squashed.patch` | The framework half: `RetroPortingToolKit/psxrecomp` branch `feat/mmx6-adaptive-renderer` @ `39ee7d79`, squashed against its merge-base. Applies cleanly to our pin `19b5a65f`; the result equals `git merge-tree` of master and that branch. 58 files, +6594/−95. |
| `materialize.sh` | Builds a patched framework copy and an R4 build against it, beside the stock ones. |
| `mmx6-reference/` | The MMX6 game half (`feat/mmx6-adaptive-renderer` @ `e0a7cb3`): widescreen plugin, adaptive background, tests, docs, mod manifest, and MMX6's LICENSE. Reference only — never compiled. |

Why a patch and not a submodule branch: the framework branch carries merge
conflict resolutions, so no per-commit series applies to current master, and
bumping our `psxrecomp` gitlink to the branch would put the default 4:3 build on
unreviewed renderer changes.

## Framework features it adds (reusable for R4)

- `ws_view_anchor.h` + `gr_wide_set_view()` — independent world/HUD origins for
  the native-wide present (software, OpenGL and Vulkan backends).
- Function filters (`psx_mod_register_function_filter_plugin`): a plugin can
  skip a guest function body. Overlay ABI 23 → 24.
- `[widescreen.cull] bias_lower_sites`, `gpu_ws_set_native_scene_predicate`
  (force native 4:3 for a known scene), screen masks (`ws_screen_mask.h`).
- MMX6-only (2D tile engine, not usable for R4): view anchoring via bg2d layer
  fields, host tile packets, mirrored panoramas.

R4 is 3D, so its widescreen is GTE projection widening, not MMX6's extra tile
columns. Master already has squash mode (`gte_set_display_aspect`); native-wide
additionally needs R4's GTE/cull/HUD hooks (`[widescreen.cull]`, HUD tagging,
backdrop keys). The MMX6 plugin's activation skeleton (options → fixed or
adaptive aspect) and its manifest are the templates for `r4.enhancement.widescreen`.

## Using it

```bash
renderer/adaptive/materialize.sh --configure
cmake --build build-adaptive/build --target psx-runtime
build-adaptive/build/r4-runtime --game build-adaptive/game.adaptive.toml
```

`--configure` builds the patched recompiler, regenerates OpenBIOS and the game
C into `generated-adaptive/`, writes `build-adaptive/game.adaptive.toml`, prints
the `PSX_OVERLAY_AUTOCOMPILE_CMD` that compiles overlay shards with the patched
toolchain (ABI-23 shards from the stock one would be rejected), and configures `build-adaptive/build` with
`-DPSXRECOMP_ROOT=build-adaptive/psxrecomp -DR4_GENERATED_DIR=generated-adaptive
-DR4_ADAPTIVE_RENDERER=ON`. All outputs are gitignored.

R4's own adaptive plugin sources go in `renderer/adaptive/src/` (compiled only
with `R4_ADAPTIVE_RENDERER=ON`), never `src/mods/`, which the stock build globs.

## Constraints

- Savestates and native overlay caches are not interchangeable between the
  stock and adaptive builds (different codegen hash and ABI). Memory cards are.
- Netplay clears all mods, so sessions always run vanilla 4:3; the adaptive
  renderer was validated only with netplay off.
- When the `psxrecomp` gitlink moves, re-verify with
  `git -C psxrecomp apply --check "$PWD"/renderer/adaptive/framework-patches/*.patch`
  and update `BASE`.

## License

`mmx6-reference/` and the framework patch are © 2026 Matthew Stanley under the
PolyForm Noncommercial License 1.0.0 (`mmx6-reference/LICENSE`).

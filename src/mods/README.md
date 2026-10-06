# R4 trusted mod plugins

Game-owned C/C++ that is statically linked into the runtime and selected by a
package manifest in `mods/preloaded/packages/` through its stable plugin id.
Archives never carry or load native code.

Rules (same as MegaManX6Recomp):

- One feature domain per source file; `CMakeLists.txt` globs `src/mods/*.c`
  and `*.cpp`.
- A feature runs only while the resolved mod plan activates its package:
  callbacks and entry hooks do nothing until then, so a disabled feature
  leaves the game byte-identical to stock. Features are default-disabled
  unless the owner decides otherwise: widescreen and frame rate are on by
  default (`default_enabled = true` in their manifests); a player turns them
  off on the launcher's Mods page.
- Keep game logic in pure headers (`r4_*_*.h`) so `tests/` can check it
  without a game build; register every test with ctest in `CMakeLists.txt`,
  inside its `R4_BUILD_TESTS` block and behind `if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/...")`
  (the release zip has no `tests/` or `tools/`, and the setup-host
  packager's CMakeLists gate only accepts that exact guard form).
- `r4_widescreen_plugin.c`: `r4.enhancement.widescreen` (`docs/WIDESCREEN.md`).
- `r4_frame_rate_plugin.c`, `r4_interp.c`: `r4.enhancement.frame-rate`
  (README "Frame rate").
- See `psxrecomp/docs/MOD_PACKAGES.md`.

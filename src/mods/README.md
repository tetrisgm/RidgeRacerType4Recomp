# R4 trusted mod plugins

Game-owned C/C++ that is statically linked into the runtime and selected by a
package manifest in `mods/preloaded/packages/` through its stable plugin id.
Archives never carry or load native code.

Rules (same as MegaManX6Recomp):

- One feature domain per source file; `CMakeLists.txt` globs `src/mods/*.c`
  and `*.cpp`.
- Every feature is default-disabled and enabled only from the launcher's Mods
  page. Callbacks do nothing until their package's activation runs.
- Keep game logic in pure headers (`r4_*_*.h`) so `tests/` can check it
  without a game build; register every test with ctest in `CMakeLists.txt`,
  inside its `R4_BUILD_TESTS` block and behind `if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/...")`
  (a checkout without `tests/` or `tools/` must still configure).
- `r4_widescreen_plugin.c`: `r4.enhancement.widescreen` (`docs/WIDESCREEN.md`).
- See `psxrecomp/docs/MOD_PACKAGES.md`.

# R4 trusted mod plugins

Game-owned C/C++ that is statically linked into the runtime and selected by a
package manifest in `mods/preloaded/packages/` through its stable plugin id.
Archives never carry or load native code.

Rules (same as MegaManX6Recomp):

- One feature domain per source file; `CMakeLists.txt` globs `src/mods/*.c`
  and `*.cpp`.
- Every feature is default-disabled and enabled only from the launcher's Mods
  page.
- Adaptive-renderer plugin sources do not go here — they need the patched
  framework and live in `renderer/adaptive/src/` (see
  `renderer/adaptive/README.md`).
- See `psxrecomp/docs/MOD_PACKAGES.md`.

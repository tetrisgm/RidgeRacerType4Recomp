# Third-party notices

## MegaManX6Recomp (Matthew Stanley) — PolyForm Noncommercial 1.0.0

This repository's layout, conventions and some tooling are adapted from
[mstan/MegaManX6Recomp](https://github.com/mstan/MegaManX6Recomp),
Copyright (c) 2026 Matthew Stanley, licensed under the PolyForm Noncommercial
License 1.0.0 (`MegaManX6Recomp.PolyForm-Noncommercial-1.0.0.txt`,
<https://polyformproject.org/licenses/noncommercial/1.0.0>).

Files copied or adapted from it:

- `tools/mdis.py`, `tools/dumpgrep.py` (copied; port/usage adjusted)
- `tools/dbg.py`, `tools/stackwalk.py` (rewritten from the MMX6 originals)
- `ghidra/instructions.txt`, `.mcp.json.example` (adapted)

The widescreen mod (`src/mods/r4_widescreen_*`,
`mods/preloaded/packages/r4.enhancement.widescreen/`) follows the
custom-renderer pattern of TombaRecomp and MegaManX6Recomp (a default-off mod
package that activates a trusted plugin). It is written fresh against
psxrecomp's `mod_plugins.h`; no code is copied from either project, so it is
MIT like the rest of this repository.

## dogewow2048 — R4 (JP) "60 FPS + 16:9" patch analysis

The widescreen mod's HUD producer classes and anchoring thresholds and its
race-scene test are derived from an analysis of dogewow2048's R4 (Japan)
60 FPS / 16:9 cheat, re-mapped to the US executable. Credit to dogewow2048.
The cheat itself and any Japanese-version extracts are not part of this
repository.

## psxrecomp and recomp-ui

The `psxrecomp/` and `recomp-ui/` submodules carry their own licenses
(`psxrecomp/LICENSE`, `recomp-ui/LICENSE`). Builds stage PCSX-Redux OpenBIOS
with its MIT notice as `bios/OpenBIOS.LICENSE` beside the executable.

Release zips carry psxrecomp's notices in `licenses/` (`psxrecomp-LICENSE`,
`THIRD_PARTY_ATTRIBUTION.md`, the libraries the runtime links), recomp-ui's
MIT license as `licenses/recomp-ui-LICENSE`, and recomp-ui's font and image
notices as `assets/fonts/NOTICE.md` and `assets/img/NOTICE.md` (OpenMoji,
CC BY-SA 4.0; Noto Sans Symbols 2 and the Noto Color Emoji flag sheet,
SIL OFL 1.1). The launcher's Lato Latin fonts (`assets/fonts/LatoLatin-*.ttf`)
are Copyright (c) 2011-2015 by tyPoland Lukasz Dziedzic, SIL Open Font
License 1.1 (<https://scripts.sil.org/OFL>); each font file also carries its
copyright and license in its own metadata.

## Other libraries in the executable

`r4-runtime` also links these statically; release zips carry their notices in
`licenses/`:

- [Dear ImGui](https://github.com/ocornut/imgui) (recomp-ui's launcher UI),
  Copyright (c) 2014-2025 Omar Cornut, MIT: `dear-imgui-LICENSE.txt`.
- [recomp-net](https://github.com/RetroPortingToolKit/recomp-net) and
  [retcomm-rbengine](https://github.com/RetroPortingToolKit/rbengine)
  (netplay), MIT: `recomp-net-LICENSE`, `retcomm-rbengine-LICENSE`.
- Windows only: the MinGW-w64 C runtime and winpthreads, from MSYS2's
  `mingw-w64-x86_64-crt` and `mingw-w64-x86_64-winpthreads`:
  `mingw-w64-runtime-COPYING.txt`, `winpthreads-COPYING`. libgcc and
  libstdc++ are linked under the GCC Runtime Library Exception.

SDL3, tinyfiledialogs and, on Windows, zlib (all zlib license), and
stb_truetype / stb_image_write (public domain option) ask for no notice in
binary copies.

## TinyCC (Windows release zips) — LGPL-2.1

Windows release zips carry the unmodified TinyCC 0.9.27 win64 binaries in
`overlay_toolchain/tcc/` (the `tcc-0.9.27-win64-bin.zip` psxrecomp's
`tools/release_stage.py` pins), by Fabrice Bellard and contributors, licensed
under the GNU Lesser General Public License 2.1 (psxrecomp's
`runtime/licenses/TinyCC-LICENSE.txt`, which its packager ships as
`overlay_toolchain/tcc/COPYING`). The game runs it as a separate
program to compile code overlays; nothing links against it. Its complete
source, `tcc-0.9.27.tar.bz2`, is attached to the same release page as the
Windows zip, and is also at
<https://download.savannah.gnu.org/releases/tinycc/tcc-0.9.27.tar.bz2>.

## Box art

`recomp/launcher/boxart.*` comes from
[libretro-thumbnails](https://github.com/libretro-thumbnails/libretro-thumbnails)
(`Named_Boxarts`); see `recomp/launcher/BOXART_SOURCE.txt`.

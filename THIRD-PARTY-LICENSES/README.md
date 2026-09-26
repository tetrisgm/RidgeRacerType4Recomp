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
- `ghidra/instructions.txt`, `annotations/SLUS_007.97_annotations.csv` header,
  `.mcp.json.example` (adapted)
- `renderer/adaptive/` — the adaptive renderer framework patch and the
  `mmx6-reference/` game-side sources (carried verbatim; see
  `renderer/adaptive/README.md`)

## psxrecomp and recomp-ui

The `psxrecomp/` and `recomp-ui/` submodules carry their own licenses
(`psxrecomp/LICENSE`, `recomp-ui/LICENSE`). Builds stage PCSX-Redux OpenBIOS
with its MIT notice as `bios/OpenBIOS.LICENSE` beside the executable.

## Box art

`recomp/launcher/boxart.*` comes from
[libretro-thumbnails](https://github.com/libretro-thumbnails/libretro-thumbnails)
(`Named_Boxarts`); see `recomp/launcher/BOXART_SOURCE.txt`.

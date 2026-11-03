# HD HUD

`r4.enhancement.ui-fonts` (Mods > Visual > **HD HUD**, on by default) draws the
race HUD from high-resolution artwork: **HD HUD by
[Kuid0us](https://github.com/Kuid0us/T4HDHUD)**, bundled in
`mods/preloaded/packages/r4.enhancement.ui-fonts/1.0.0/pack/`. It uses
psxrecomp's HD texture packs (`psxrecomp/docs/HD_TEXTURE_PACKS.md`): the
package's `pack` folder resource defaults to the bundled folder and runs the
framework `psx.hd-textures` plugin; no R4 code is involved.

How it works: each PNG is keyed `TTTTTTTT-PPPPPPPP` (CRC-32 of the texture
words the game uploads, CRC-32 of the 16-colour palette a draw uses). When a
HUD sprite samples a tracked upload with a matching key, OpenGL samples the
PNG instead, at the internal resolution. Guest VRAM, readbacks, savestates and
netplay keep the original pixels (presentation only). Software and Vulkan
show the original HUD.

## Pointing it at another pack

Mods > Visual > HD HUD > **Change folder** selects any folder of
`TTTTTTTT-PPPPPPPP.png` files (or a DuckStation `texupload`/`texpage` pack).
The selection is stored in `mods/state.toml` and wins over the bundled pack:

```toml
[[feature]]
package_id = "r4.enhancement.ui-fonts"
id = "ui-glyphs"
enabled = true
[feature.resources]
pack = "/path/to/your/pack"
```

## US keys

T4HDHUD names its PNGs for the Japanese game; the author notes they may need
renaming for another version. Tracing live uploads in the US game
(SLUS-00797) showed:

| Upstream (JP key) | US | Content |
|---|---|---|
| `275d0836`, `635e2d0a` (7-segment digits), `2edf11dc` (RPM gauge), `1477dbf9` (red-zone ticks), `185689d1` and the other seven course maps, `75c13c85` (markers), `b18b2859` (logo) | same upload CRC | used as they are |
| `c41643fd-*` (text and digit atlas) | `32203df6-*` | the US atlas has `mph` where the JP one has `km/h`; every other glyph is in the same place |

`tools/r4_hd_hud_pack.py` builds the bundled pack from the upstream folder
and native reference images (the US uploads decoded with each palette):

- copies the files whose keys already match;
- builds `32203df6-{bf9158c6,3cba29b4,db4fb359}` from the JP atlas art per
  palette. A glyph the art leaves empty in one palette is taken from another
  palette's art, recoloured through the native palettes; `mph`, the `rpm`
  plate, `/`, a punctuation mark and a small `"` mark are not in the art and
  are 8x Scale2x upscales of the US glyphs;
- adds the palette variants the US game draws that upstream lacks (gauge
  ticks lit and unlit, course-map shadow, logo fade steps), recoloured from
  the upstream art.

## Coverage

Checked in a Grand Prix race on the owner test build (OpenGL, Match display
10x, widescreen Fit, dynamic resolution 3x/6x/10x, Smooth motion on, 0
dispatch misses), stock and pack captured at the same moment:

- HD: RANK / RECORD headings, rank digits, record and lap times, TIME LIMIT
  counter, speed digits, gear digit, RPM ticks, red zone and dial labels,
  course map and its shadow.
- Upscaled (Scale2x, not hand-drawn): `mph`.
- Still native: the start lights and countdown, menu and results
  fonts (an English menu atlas has no upstream art), car and track textures.
- Not yet checked: VS split screen and the other seven course maps (their
  upload CRCs are the JP ones, unverified in the US game).

Savestates: saving a slot writes the HUD's upload residency beside it
(`*.hdres`); loading that slot restores it, so the HUD stays HD after a
load. States saved without the sidecar show the original HUD until the next
race loads it again.

## Red-zone coverage correction, 2026-11-03

The owner reported hollow, fragmented red RPM bars. The exact slot-4 scene
matches both bundled keys: dial `2edf11dc-cf63f4a4` and red overlay
`1477dbf9-38bfad7b`. The dial artwork contains solid red bars; the separate
overlay is intentionally transparent. This was not a missing palette image.
The renderer clipped replacement artwork against zero texels in the original
texture, punching the native dial's stippled silhouette through the HD bars.

Framework commit `3a0502c5` lets replacement alpha control coverage while
retaining native STP for blending and unchanged guest VRAM. No artwork changed.
The real GL fixture passes 233 full-surface, 219 windowed and 19 native-baseline
checks, including opposite native/replacement coverage. The game build passes
26/26 CTests and renders solid red bars at 4K internal resolution in 21:9 and
4:3, stationary, accelerating and after current-build save/load. PGXP remains
enabled; dispatch and segment misses remain zero. Ignored evidence is under
`analysis/feedback/hud-redline/`; binary SHA-256 is
`557e50bb2c61807c9f228b894b871d6c0861df979e9a7496992b4d93eb6aa47e`.

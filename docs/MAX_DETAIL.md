# Max Detail: R4 at full detail

`r4.enhancement.max-detail` is **on by default** (owner decision,
2026-10-01): **Mods > Detail > Max Detail**. It removes the detail R4 drops
with distance to fit the PlayStation's budget. Every option has a Stock
choice, and with the package off the game runs the stock path unchanged
(the off-switch identity check is below). Netplay always plays stock.

| Option | Choices (default first) | What it changes |
|---|---|---|
| Draw distance | Maximum, Extended, Stock | Extended keeps the far course polygons the game drops at the end of its ordering table; Maximum also adds the visibility lists of the two track sections ahead and behind, in views narrower than about 30:9 (wider views draw what Extended draws) |
| Course detail | Always full, Stock | No distance LOD on the course: full-resolution textures and smooth shading at every distance, mirror included |
| Car detail | Always full, Stock | Full car models (3D wheels, full-resolution texture) out to the car draw distance; mirror cars as far as ahead |
| Split screen | Same as 1P, Stock | VS halves use the 1P course subdivision and car models |

## How it works

| Piece | Where | What |
|---|---|---|
| Course detail | plugin hook on the course renderer `0x80060F94` | Every course polygon whose depth is past scratch `0x1F80005C` (rewritten every frame; default 5120, -1 = all in the mirror) is drawn flat-shaded, with UVs halved onto the half-size texture copy (record `+0x24` bit 0). The hook writes `0x7FFF` there before each course draw (main, alternate, mirror) |
| Split screen | same hook | The VS handler sets scratch `0x1F80005E = 1` before each viewport's course draw, which picks coarser subdivision thresholds (`0x11C/0x1FC/0x2CC/0x38C` instead of 1P's `0x180/0x260/0x330/0x3F0`). The hook writes 0 |
| Car detail | manifest `[[patch]]`, car table `0x8009F228`; VBlank callback | Five rows (1P, mirror, TV/replay, 2P, 2P alternate) of three s16 distances: full model below T0, simpler model below T1, any model below T2. Always full: `8704, 8704, 8704` (mirror `672, 8704, 8704`). Split screen alone copies the 1P row into both 2P rows. T2 stays 8704 (see Limitations). With Car detail = Stock, a VBlank callback puts the stock rows back if a save state brought in rows this package writes |
| Draw distance: far clamp | `game.toml [[draw_distance.clamp]]`, psxrecomp | The 18 course renderers reject a polygon whose OT index `OTZ >> 5` (in `$v0`) is 448 or more (`sltiu t2,v0,0x1C0` x16, `addiu at,v0,-1; sltiu at,at,0x1BF` x2). The slot is then `128 + v0 + bias`, so raising the limit could pass the end of OT1 into OT2. psxrecomp's opt-in clamp (on from Extended) clamps `v0` to `0x1BF` before the guard: the polygon is kept in the farthest slot, and no slot moves past the stock worst case. `tools/r4_detail_scan.py` derives the list from the EXE and checks that `v0` reaches the slot unchanged |
| Draw distance: visibility | `r4_pvs.h`, hooks `0x8006F584` / `0x8007166C` | The course is drawn from a block list per (section, heading octant), `table[section * 8 + octant]`; a section is five track segments (`*0x800AC084` segments, wrapping). Maximum appends the lists of sections +/-1 and +/-2 (nearest first, capped at 255 blocks). With the widescreen package live it also adds those sections' wide octants; the widescreen plugin adds the camera section's. From about 30:9 (two wide octants each side) Maximum adds no sections: there the cross product overran the PS1 frame budget (Verification) |

Both scratch values are rebuilt by the game every frame and the course list is
rebuilt every frame, so nothing the plugin writes outlives a frame it did not
run. The clamp switch is psxrecomp host state, reset to off at every session
start; only this package's activation turns it on. The car table is the one
persistent write (Limitations).

### A fixed widescreen bug

The widescreen plugin's course-list union indexed the table with 9 columns
per section (`section * 9 + octant`). The game indexes it with 8
(`0x8006F5AC`: `sll s0,s0,3; addu s0,s0,v0`), so from section 1 on it added
the blocks of other sections and octants. A probe at the list's first
consumer matched the game's own entry with 8 columns on 1200 of 1200 course
draws and with 9 on none. `r4_pvs.h` now uses 8 for both plugins, and holds the
hook addresses both use. `tools/r4_detail_scan.py --check` (ctest
`r4_max_detail_sites`) checks the stride, the table pointer and the hooks'
return-address gates against the game's code at `0x8006F584`-`0x8006F5E8`, and
`test_r4_max_detail` lays its mock table out with the game's stride, so a wrong
column count fails both.

## Limitations

- The car draw distance (T2 = 8704) is not raised. Past it R4's car transform
  overflows: a car about 15000 units away drew huge and misplaced above the
  grandstand in the research runs.
- The mirror keeps its stock block list. Lifting it shows the grandstands and
  road behind, but with everything else at maximum the PS1 game logic fell to
  28.8 frames/s (stock: 30). Not offered.
- From about 30:9 (Fit in a window that wide, or the 32:9 View) Maximum draws
  what Extended draws: the neighbouring sections' wide octants, with full car
  models and a full grid ahead, overran the PS1 frame budget.
- Measured on Helter Skelter (Grand Prix start grid, VS start) and the two
  attract-demo courses only. The other courses are untested: a heavier course
  could overrun the PS1 frame budget (the game then runs below 30 frames/s,
  as an overloaded PS1 would) or the primitive heap.
- Car reflections stay as on stock (on in replays and the attract demo, off
  in live races). Turning them on in races is an owner decision.
- 1P course subdivision stays stock: doubling it cracked wall seams and
  raised the primitive heap from 34 % to 55 %.
- Far polygons share the farthest ordering-table slot, so among themselves
  they are drawn in submission order rather than by depth. They are far and
  small; no wrong overlap was seen.
- With Maximum, blocks the game's visibility bake judged hidden can be drawn
  where a gap lets them show.
- The car table is written once (manifest patches), so a save state keeps the
  table it was made with. Loaded with the package on and Car detail = Stock,
  the plugin puts the stock rows back at the next VBlank. Loaded with the
  package off, nothing runs: a state made with Always full keeps full car
  models until the game is restarted. (Car detail = Always full re-applies its
  rows to any state, as every plan does.)
- The game never checks its primitive heap. The highest high-water measured
  with this package's defaults is 79.1 % (2P at 32:9), 81.1 % with an earlier
  one-section setting; heavier courses or other packages that add primitives
  leave less headroom. `R4_MD_TRACE=1` logs the high-water mark, overflow hits
  and the highest occupied ordering-table slot every 60 frames.

## Verification

Measured on macOS arm64 (Apple M4), debug-tools build, OpenGL, on a machine
shared with other work (load average 7-26), every run with 0 dispatch misses
and 0 segment misses. Only Helter Skelter and the two attract-demo courses
were measured.

**Off switch.** `psxrecomp/tools/fp_identity.py`, 20000 frames of the no-input
boot, intro, title and first attract race, cold overlay cache: the package off
is IDENTICAL to the build before this mod (every judge column, no tolerance),
and so is the package on with every option at Stock (both re-run on the final
build, after the review fixes).

**Game frame rate** is PS1 game frames per emulated second (30 is full speed),
counted from the game's main-loop counter against emulated VBlanks, so it
measures the emulated PS1's frame budget, not the host. "Lost" is game frames
short of 30 per second over the run. Primitive heap high-water is of
`0x21018` bytes (the game never checks it). Savestates on Helter Skelter, held
at the accelerator from the start: the Grand Prix grid (rank 8 of 8, seven
cars ahead, the heaviest scene found) for 2400 VBlanks, the VS start (both
pads) for 3000. 4K unless noted; 29:9 is Fit with the window aspect set to
29:9, close to the widest view that still takes two sections (from 30:9 it
takes none).

| View | Stock | Max Detail (defaults) |
|---|---|---|
| 1P grid, 4:3 | 29.97, lost 0; heap 42.0 % | 29.96, lost 0; heap 44.9 % |
| 1P grid, 16:9 | 29.97, lost 0; heap 46.3 % | 29.98, lost 0; heap 49.6 % |
| 1P grid, 21:9 | 29.94, lost 0; heap 50.8 % | 29.96, lost 0; heap 53.7 % |
| 1P grid, 29:9 (Fit) | 29.96, lost 0; heap 58.1 % | 29.96, lost 0; heap 60.7 % |
| 1P grid, 32:9 | 29.96, lost 0; heap 59.8 % | 29.96, lost 0; heap 61.6 % |
| 2P VS start, 4:3 | | 29.97, lost 0; heap 59.0 % |
| 2P VS start, 4:3, 8K | | 29.97, lost 0; heap 57.7 % |
| 2P VS start, 21:9 | | 29.97, lost 0; heap 69.3 % |
| 2P VS start, 29:9 (Fit) | 29.97, lost 0; heap 59.3 % | 29.97, lost 0; heap 78.0 % |
| 2P VS start, 32:9 | 29.98, lost 0; heap 62.2 % | 29.98, lost 0; heap 79.1 % (two runs: 75.6, 79.1) |
| Attract demo, 32:9, both races (9280 VBlanks) | 29.97, lost 0; heap 62.7 % | 29.96, lost 1 (within a frame of stock); heap 66.7 % |

Why Maximum adds no sections from about 30:9: on the 1P grid at 32:9 the
earlier settings lost frames. Over the 2400 VBlanks (40 s), two sections each
way (the first version) lost 46 game frames, with 60-VBlank windows down to
21.6; one section lost 14 (down to 24.0); two sections with only the heading's
neighbouring octants lost 13; one section with those octants lost 1; none lost
0. The attract demo at 32:9 with two sections lost 51 game frames by frame
14000 (stock: 0). Up to 29:9 two sections lost nothing in 1P or 2P. In 2P at
32:9 one section each way (the first version's split-screen setting) lost 1 to
6 frames per 3000 VBlanks and reached 80-81 % heap.

Earlier runs from savestates on an empty road (no cars in view), for the
record: 1P 4K 29.97 (heap 40.7 % stock, 43.6 % Max Detail), 1P 8K 29.97, 2P 4K
29.97 (46.8 %, 57.5 %), 2P 8K 29.97; the highest occupied OT1 slot was 519 in
1P, 630 in 2P and 665 in the attract demos (the same as stock there), against
a stock worst case of 702. At the grid it is 650 with the package off and on.
Primitives per frame: 1P 694 to 741, 2P 903 to 1159. Host GPU scene time
stayed within run-to-run noise at 4K and 8K.

**Frame Rate.** From a Grand Prix savestate with Max Detail on, Interpolated
120 is IDENTICAL to Frame Rate off over 1428 frames (per-frame fingerprints),
and with `PSX_RENDER_PASS_VERIFY` 935 of 935 passes verify. The plugin's hooks
run inside the passes, so in-between frames are drawn at the same detail.

**Garbage check.** Both attract demo races (a full course each, chase and TV
cameras) at 4K, a frame every 120: no misplaced or corrupt geometry; a brown
panel in one tunnel is stock content (it shows with the package off too).
Grand Prix and VS savestates at 4K and 8K, 4:3 and 21:9, 1P and 2P: no
garbage, 0 overflow hits.

**Save states.** A Grand Prix state saved with the defaults (full car rows),
loaded with Car detail = Stock: the 1P, mirror and TV rows are stock again
after one VBlank, and with Split screen = same the 2P rows hold the 1P row
(the plan's own writes, applied without a guard failure); with Split screen =
Stock all five rows are stock. Loaded with the package off, the full rows stay
(Limitations).

**Tests.** `tools/r4_detail_scan.py --check game.toml` (ctest
`r4_max_detail_sites`, needs the disc) re-derives the 18 clamp sites from the
disc's EXE, checks the manifest's car-table guards, and checks `r4_pvs.h`
against the game's course-list lookup and the hooks' call sites.
`tools/r4_detail_scan.py --check-manifest` (ctest `r4_max_detail_manifest`, no
disc) fails unless the package is on by default with every option at its most
detail, every option offers Stock, and the car patches are exactly the tables
in `r4_max_detail.h`. `tests/test_r4_max_detail.c` (ctest `r4_max_detail`)
covers the options, the section reach per view width, car tables and their
restore after a save state, the shared course-list merge (on a mock table laid
out with the game's 8-octant stride) and the plugin against a mock mod API,
both return-address gates included. `R4_MD_SECTIONS=n` overrides the section
reach for A/B runs.

## Netplay

Every netplay launch clears the mod plan (`mod_runtime_clear_for_netplay`),
including packages that are on by default, and the clamp switch is reset at
every session start, so both peers always play stock. A default-on package is
therefore never a desync risk; it is also never active online.

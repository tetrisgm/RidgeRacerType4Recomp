# Max Detail: R4 at full detail

`r4.enhancement.max-detail` is **on by default** (owner decision,
2026-10-01): **Mods > Detail > Max Detail**. It removes the detail R4 drops
with distance to fit the PlayStation's budget. Every option has a Stock
choice, and with the package off the game runs the stock path unchanged
(the off-switch identity check is below). Netplay always plays stock.

| Option | Choices (default first) | What it changes |
|---|---|---|
| Draw distance | Maximum, Extended, Stock | Extended keeps the far course polygons the game drops at the end of its ordering table and draws cars to 14000 instead of 8704; Maximum also adds the visibility lists of up to twelve track sections ahead (one behind), as many as fit the PS1 frame budget (Far draw distance) |
| Course detail | Always full, Stock | No distance LOD on the course: full-resolution textures and smooth shading at every distance, mirror included |
| Car detail | Always full, Stock | Full car models (3D wheels, full-resolution texture) out to the car draw distance; mirror cars as far as ahead |
| Split screen | Same as 1P, Stock | VS halves use the 1P course subdivision and car models |
| Car reflections | On, Stock | Reflective car bodies in live races too (stock R4 shows them only in the fly-by, replays and the attract demo). For now in 4:3 views only: widened views keep Stock (Limitations) |
| Mirror scenery | Stock, Full | **Off by default.** Full draws the rear-view mirror's whole course list instead of its first few blocks; it costs the emulated PS1 more time than any other option (Verification) |

## How it works

| Piece | Where | What |
|---|---|---|
| Course detail | plugin hook on the course renderer `0x80060F94` | Every course polygon whose depth is past scratch `0x1F80005C` (rewritten every frame; default 5120, -1 = all in the mirror) is drawn flat-shaded, with UVs halved onto the half-size texture copy (record `+0x24` bit 0). The hook writes `0x7FFF` there before each course draw (main, alternate, mirror) |
| Split screen | same hook | The VS handler sets scratch `0x1F80005E = 1` before each viewport's course draw, which picks coarser subdivision thresholds (`0x11C/0x1FC/0x2CC/0x38C` instead of 1P's `0x180/0x260/0x330/0x3F0`). The hook writes 0 |
| Car detail | manifest `[[patch]]`, car table `0x8009F228`; VBlank callback | Five rows (1P, mirror, TV/replay, 2P, 2P alternate) of three s16 distances: full model below T0, simpler model below T1, any model below T2. Always full: `8704, 8704, 8704` (mirror `672, 8704, 8704`). Split screen alone copies the 1P row into both 2P rows. T2 stays 8704 (see Limitations). With Car detail = Stock, a VBlank callback puts the stock rows back if a save state brought in rows this package writes |
| Draw distance: far clamp | `game.toml [[draw_distance.clamp]]`, psxrecomp | The 18 course renderers reject a polygon whose OT index `OTZ >> 5` (in `$v0`) is 448 or more (`sltiu t2,v0,0x1C0` x16, `addiu at,v0,-1; sltiu at,at,0x1BF` x2). The slot is then `128 + v0 + bias`, so raising the limit could pass the end of OT1 into OT2. psxrecomp's opt-in clamp (on from Extended) clamps `v0` to `0x1BF` before the guard: the polygon is kept in the farthest slot, and no slot moves past the stock worst case. `tools/r4_detail_scan.py` derives the list from the EXE and checks that `v0` reaches the slot unchanged |
| Car reflections | plugin hook on the env-map car part draw `0x80014A90` | Car body parts with the environment-map flag go through `0x80014A90`, which draws them with the env map from texture page `*0x8009E288`, or returns at once when the page is negative (the caller then draws the plain mesh). The game sets 10 at race init (the fly-by, `0x8003D3E0`), after the goal (`0x8002A3E4`) and every replay and attract-demo frame (`0x8005EB78`), 25 in the garage and car select (`0x8003763C`, `0x80055C7C`), and -1 at the start signal of every live race (the race overlays) and in the menus. At the hook, in a live race (the widescreen plugin's race test, a race handler resident, and race phase 1-3: countdown and racing), only over -1 and only while the view has no widescreen margin (`psx_mod_widescreen_x_margin() == 0`), the plugin writes 10. The draw reads the page twice after the hook, with no store between, so the part is drawn with it. If the view turns wide during the race (Fit and a resized window), the plugin puts back the -1 it replaced; a page the game set itself is never touched. The reflective part replaces the plain one, but each part is drawn as the body plus an additive env-map pass, with the GPU's set-mask bit (GP0 E6h) toggled around it: on the Grand Prix grid about 400 mask toggles and +2.5 points of primitive heap a frame |
| Mirror scenery | plugin hooks on the mirror's block limit `0x80071704` and its list's first consumer `0x8006EDEC` | The mirror draw `0x8006F0C8` builds its course list (`0x8006EB58`, the same per-section, per-octant table as the main view), asks `0x80071704` for a per-section limit (`(u8)(*0x8010E8C8)[section * 8 + octant]`, mostly 4-9 blocks), lowers the list's count word to it (`0x8006F100`-`0x8006F114`: only the count; the block pointers past it stay), sets up the GTE and hands the list to `0x8006EDEC`. With Full the plugin reads the built count at the limit's entry and puts it back at the consumer's entry. Both hooks gate on the mirror draw's return addresses, so the main course draws are untouched |
| Draw distance: visibility | `r4_pvs.h`, hooks `0x8006F584` / `0x8007166C` | The course is drawn from a block list per (section, heading octant), `table[section * 8 + octant]`; a section is five track segments (`*0x800AC084` segments, wrapping). Maximum appends the lists of sections +/-1 and +/-2 (nearest first, capped at 255 blocks). With the widescreen package live it also adds those sections' wide octants; the widescreen plugin adds the camera section's. From about 30:9 (two wide octants each side) Maximum adds no sections: there the cross product overran the PS1 frame budget (Verification) |

Both scratch values are rebuilt by the game every frame and the course lists
(main and mirror) are rebuilt every frame, so nothing the plugin writes there
outlives a frame it did not run. The clamp switch is psxrecomp host state,
reset to off at every session start; only this package's activation turns it
on. Two writes persist: the car table (Limitations), and the reflection page,
which keeps 10 until the game next sets it. Stock leaves 10 there after every
finished race too (the after-goal run sets it), and the next race start, the
menus or the garage set their own value, so a 10 left by a race quit from the
pause menu is a state stock R4 already reaches.

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

## Far draw distance (Maximum)

What popped in with the earlier Maximum (two sections each way): course
blocks entering the list at 8k-12k units (world >> 8; `R4_MD_TRACE=2` pop
census), cars vanishing at the 8704 cull, and trackside animated objects
(per-object segment windows, `0x800720F0`; not changed yet).

| Piece | Where | What |
|---|---|---|
| Direction | `r4_pvs.h` `r4_pvs_ahead_dir`, `r4_pvs_merge_dir` | The centreline (`*0x800AC05C`, 0x3C per segment, x/z at +8/+0xC) and the camera yaw give which way is ahead; sections are added ahead (up to 12) and one behind, nearest first |
| View cone | `r4_md_block_bounds` / `r4_md_block_visible` | Added blocks are kept only if their bounding circle meets the view cone and lies within 30000 of the camera (GTE IR range); host-side, so free for the PS1. The game's own list is never filtered |
| Frame budget | hooks `0x8009375C` (ra `0x8001E7F0`) and VSync (ra `0x8001E7D0`) | Busy guest cycles per frame steer a level -2..10 (ahead = 2 + level, far cars from 1): down 2 above 96 %, down 1 above 93 %, up 1 after 20 frames under 86 %. Deterministic; reset by save-state loads |
| Far objects | hooks `0x8006F160`, SetTransMatrix `0x80091320` (ra `0x8006F1E8`) | `0x8006F160` stores object - camera as a 16-bit SVECTOR, the real limit behind the 8704 car cull. When the delta does not fit, the exact MVMVA translation is recomputed from the 32-bit delta; out of the GTE-safe range the object is moved behind the camera. In range nothing changes |
| Far cars | hooks `0x8002DC00` / `0x80015F60` | The 1P/TV/2P rows' T2 becomes 14000 only for each car's lookup (written at entry, put back after the last row read on all four paths), so the table in RAM and in save states stays as the manifest wrote it. Past 8704 the game's simplest model; the bound is the car OT guard (`0x8005F6BC`, 447 << 5) and SZ |

Measured (Helter Skelter, autopilot, 40 s, 0 dispatch/segment misses; CPU =
frame start to DrawSync, % of two VBlanks, mean/max):

| Run | Package off | Earlier Maximum (±2) | New default |
|---|---|---|---|
| Grid, 4:3 | 68/84 %, lost 0 | 80/96 %, heap 49.5 %, lost 0 | 85/96 %, heap 54.7 %, lost 0, ~10 sections ahead |
| Lap, 4:3 | 70/79 % | 75/86 %, heap 47.1 % | 82/96 %, heap 53.3 %, lost 0, ~10 ahead |
| Grid, 16:9 | 72/85 % | 82/95 %, heap 51.8 % | 85/102 % (one frame over), heap 53.8 %, lost 0, ~3-4 ahead |
| Lap, 16:9 | 73/81 % | 78/89 %, heap 51.1 % | 85/97 %, heap 55.6 %, lost 0, ~7 ahead |

OT1 high-water stays 645-650 (stock worst case 702). Pop census at 4:3:
pops under 12000 units fell from 78 to 10 per 40 s; most now land at
12k-30k. 2P was not re-measured (no VS save state in this build).

## Limitations

- The car draw distance (T2 = 8704) is not raised. Past it R4's car transform
  overflows: a car about 15000 units away drew huge and misplaced above the
  grandstand in the research runs.
- Mirror scenery is off by default (owner decision). Full is the one option
  that costs the emulated PS1 frames: on the Grand Prix grid (Helter
  Skelter, seven cars ahead) it lost 4 game frames in 40 s at 4:3 and 8 at
  16:9, with 60-VBlank windows down to 27.0 and 24.6 game frames/s, and
  raised the primitive heap by about 9 points (Verification). Stock is the
  default for that reason.
- Car reflections stay stock in widened views (any widescreen margin), for
  now. The reflective parts toggle the GPU's set-mask bit (GP0 E6h) around
  every part, which splits psxrecomp's textured batches (about 80 to 240 a
  frame on the grid at 16:9), and its native-wide OpenGL path draws those
  batches many times slower than the 4:3 path: at 4K, 16:9, the host's
  scene GPU time went from 15-16 ms to 143-166 ms a frame, the host fell to
  6-7 emulated VBlanks/s and the game to 23-27 frames/s, where 4:3 showed
  no difference. Stock R4 already pays this in wide views: its attract demo
  shows reflections, and with this package off it ran at 5-13 VBlanks/s at
  4K and 16-29 at 4x (16:9). Independently of the host, the wide view's
  extra geometry plus reflections also overruns the PS1 budget at the start
  (28.0 game frames/s over the first 10 s on the Helter Skelter grid at
  16:9). If psxrecomp makes those batches cheap, reflections could be allowed
  in wide views where the PS1 has room; the gate is
  `r4_md_reflections_action`.
- Car reflections follow the race test of the widescreen plugin, so Extra
  Trial and link-battle races (overlays 666/667) keep stock reflections.
- From about 30:9 (Fit in a window that wide, or the 32:9 View) Maximum draws
  what Extended draws: the neighbouring sections' wide octants, with full car
  models and a full grid ahead, overran the PS1 frame budget.
- The distance options were measured on Helter Skelter (Grand Prix start
  grid, VS start) and the two attract-demo courses; Car reflections and
  Mirror scenery also on three more Grand Prix grids (Verification). A
  heavier course could overrun the PS1 frame budget (the game then runs
  below 30 frames/s, as an overloaded PS1 would) or the primitive heap.
- Car reflections cost the emulated PS1 a little time: at the Grand Prix
  start, with seven full car models ahead, the busiest frame took 93 % of
  the two-VBlank budget on Helter Skelter (87 % without reflections) and
  97 % on course type 6 (88 %), where the game dropped 1-2 frames in the
  first seconds of the race. Set Car reflections to Stock if that matters
  more than the look.
- Car reflections add primitives: each reflective part is the body plus an
  additive env-map pass. On the grid at 4:3 the heap high-water rose from
  44.9 % to 47.4 %, in 2P from 57.7 % to 59.3 %.
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
and 0 segment misses. The distance options were measured on Helter Skelter
and the two attract-demo courses only.

**Off switch.** `psxrecomp/tools/fp_identity.py`, 20000 frames of the no-input
boot, intro, title and first attract race, cold overlay cache: the package off
is IDENTICAL to the build before this mod (every judge column, no tolerance),
and so is the package on with every option at Stock (both re-run on the final
build, after the review fixes, and again with Car reflections and Mirror
scenery: off and all six options at Stock, IDENTICAL over 20000 frames).

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

**Car reflections and Mirror scenery** (re-measured 2026-10-01 on the build
with both options; every run 0 dispatch and 0 segment misses). Grand Prix
heat-1 grids saved at the countdown with the package off, rank 8 of 8 with
seven cars ahead, on course types 0-7 (the course type poked during the load,
a test-only write), plus the Helter Skelter grid and VS states above. 4K, the
accelerator held, 2400 VBlanks (VS 3000, 1200 for the VS mirror check).

| Grid | Reflections Stock | Reflections On (default) | + Mirror scenery Full |
|---|---|---|---|
| Helter Skelter, 4:3 | 29.96, lost 0; heap 44.9 % | 29.96, lost 0; heap 47.4 % | 29.88, lost 4 (window min 27.0); heap 56.1 % |
| Helter Skelter, 16:9 | 29.97, lost 0; heap 49.6 % | same (wide: stock) | 29.77, lost 8 (24.6); heap 58.9 % |
| Helter Skelter, 32:9 | | 29.96, lost 0; heap 61.6 % (wide: stock) | 29.51, lost 18 (24.0); heap 67.7 % |
| Course type 2 (night), 4:3 | 29.96, lost 0; heap 37.3 % | 29.96, lost 0; heap 40.4 % | 29.81, lost 6 (27.0); heap 57.3 % |
| Course type 4 (sunset), 4:3 | 29.98, lost 0; heap 42.2 % | 29.96, lost 0; heap 42.6 % | 29.97, lost 0; heap 49.8 % |
| Course type 6, 4:3 | 29.97, lost 0; heap 36.3 % | 29.91, lost 2 (29.0); heap 41.0 % | 29.78, lost 8 (26.0); heap 41.8 % |
| VS mid-race, 4:3 | 29.96, lost 0; heap 57.7 % | 29.98, lost 0; heap 59.3 % | 29.97, lost 0 (2P has no mirror) |

Every run repeated where it lost frames gave the same counts (the emulated
PS1 is deterministic from a savestate). The PS1's headroom, measured
directly as guest cycles from the race handler's entry to the first VSync
after DrawSync (`cyc_watch`), as a share of the two-VBlank budget
(mean / 95th percentile / busiest frame):

| Scene | Stock package | Defaults, reflections Stock | Defaults (reflections On) | + Mirror Full |
|---|---|---|---|---|
| Helter Skelter grid, 4:3, VBlanks 360-2160 | 59 / 75 / 78 % | 67 / 83 / 87 % | 68 / 85 / 89 % | 69 / 91 / 97 % |
| Helter Skelter grid start, 4:3, first 600 | | 80 / 85 / 87 % | 84 / 91 / 93 % | |
| Course type 6 grid start, 4:3, first 600 | | 80 / 86 / 88 % | 84 / 92 / 97 % | |
| Helter Skelter grid start, 16:9, first 600 | | 83 / 89 / 90 % | 88 / 98 / 98 % (forced, no gate): 28.0 frames/s | |

The forced 16:9 run gave the same cycles, to 0.1 %, at 2x (host at real
time) and at 4K (host at 12 VBlanks/s), so these costs are the PS1's, not
the host's: in wide views reflections would also cost game frames at the
start, besides the host cost below.

Reflections drawn: race overlays set the page to -1 at the start signal, the
plugin's one write per race brings 10 back (`R4_MD_TRACE`: one write per
race, 0 in wide views). `img/reflections_8_courses_refl0_vs_on.png` (all
eight course types, the cars ahead at the start), the player's car in the
chase view (`img/reflections_player_car_chase_refl0_vs_on.png`), the other
player's car in VS (`img/reflections_2p_vs_refl0_vs_on.png`): the glass and
paint of the fly-by and replays, no misplaced or flickering parts. Mirror
scenery (`img/mirror_scenery_stock_vs_full_4courses.png`): the start gate,
grandstands and buildings behind instead of empty sky and road. On course
type 2 the longest mirror list per second went from 2-6 blocks to 8-15.

**Native-wide cost of the reflective parts.** Each reflective part is drawn
as the body, GP0(E6h) set-mask on, an additive env-map polygon on texture
page (640, 0), set-mask off: about 400 E6h commands a frame on the grid.
psxrecomp's OpenGL renderer splits its textured batches at each mask change.
In 4:3 that costs nothing measurable (scene GPU 13.6 ms with and without
reflections at 4K). In the native-wide path, at 16:9 and 4K on the Grand Prix
grid, batches went from 76-82 to 184-242 a frame, CPU flush time from 4.5 to
39-61 ms and scene GPU time from 15-16 ms to 143-166 ms; the host fell to 6-7
emulated VBlanks/s (`meas/stall/`). The stock attract demo, which shows
reflections, does the same with this package off: 5-13 VBlanks/s at 4K, 16-29
at 4x and 29-36 at 2x in 16:9, against 38-54 at 4:3 in the same conditions
(another run shared the machine; `meas/attract/`). With the gate, the 16:9
grid runs with the scene GPU at 13-15 ms and the host at 58.6 VBlanks/s over
the run, and a window turned from 4:3 to 16:9 mid-race drops the reflections
at once and keeps the host at 54-60 VBlanks/s (`meas/resize/`). Evidence:
`analysis/visual-fidelity/detail-refl-mirror-evidence/` (local).

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
disc's EXE, checks the manifest's car-table guards, checks `r4_pvs.h`
against the game's course-list lookup and the hooks' call sites, and checks
`r4_max_detail.h`'s reflection and mirror constants against the page setter,
the env-map draw's two page reads, the race-init and replay calls and the
mirror draw's call sequence.
`tools/r4_detail_scan.py --check-manifest` (ctest `r4_max_detail_manifest`, no
disc) fails unless the package is on by default with the owner's option
defaults (the most detail, Car reflections On, Mirror scenery Stock), every
option offers Stock, and the car patches are exactly the tables in
`r4_max_detail.h`. `tests/test_r4_max_detail.c` (ctest `r4_max_detail`)
covers the options, the section reach per view width, car tables and their
restore after a save state, the shared course-list merge (on a mock table laid
out with the game's 8-octant stride) and the plugin against a mock mod API,
both return-address gates included; car reflections (only over -1, only in a
live race in phase 1-3, never in a wide view, our page taken back when the
view turns wide, the game's pages never touched) and mirror scenery (the
count put back only between the mirror draw's two return-address gates, once
per limit call). `R4_MD_SECTIONS=n` overrides the section reach for A/B runs;
`R4_MD_TRACE=1` also logs reflection writes and mirror list counts.

## Netplay

Every netplay launch clears the mod plan (`mod_runtime_clear_for_netplay`),
including packages that are on by default, and the clamp switch is reset at
every session start, so both peers always play stock. A default-on package is
therefore never a desync risk; it is also never active online.

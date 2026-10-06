# Online Link Battle (2-4 players)

Online play uses R4's own Link Battle: the race the original game ran between
two consoles joined by a link cable. Here every player runs one copy of the
game on their own machine and sees their own car full screen. The host is
Player 1; each player who joins takes the next free seat (P2, P3, P4). A
session of 2, 3 or 4 players races 2, 3 or 4 cars.

Local split screen stays two players: the stock VS Battle on pads 1 and 2.

## Playing

Every peer needs the same build, the same disc and the experimental switch:
start the launcher with `tools/launch_link_experimental.sh build` (it sets
`PSX_R4_LINK_EXPERIMENTAL=1`). On the NETPLAY page the host creates a room
(LAN or online) and the others join; the host presses Play. In the game,
choose **Link Battle** on the main menu. Car Select and Course Select work as
in the original; every player can confirm. In the race each player steers
their own car and sees only their own view.

Pause with Start from any seat. The shared pause menu then shows on every
screen over the two upper views; Up/Down and Start from any seat move and
confirm it (Cancel, Restart, Retire). After the race, Results offers
**Car & Course Change** for another race or **Exit** to the title.

Rewind is never available in a netplay session: `psxrecomp`'s rewind skips
capture and refuses the toggle while netplay is active
(`runtime/src/psx_rewind.c`: the capture tick, the capture and
`psx_rewind_toggle()` all check `psx_netplay_active()`), in delay and rollback
mode and for any seat count.

## Design

**Seats.** The framework assigns them; R4 only reads them. In a LAN or online
room the host holds seat 0 and each guest takes the lowest free lobby seat,
which is the join order unless a player left and a later one took the gap.
At Play the host stays session slot 0 and the others follow in lobby-seat
order; the session has one slot per seated player, so a 4-seat room started
with three players is a three-player session. With the direct CLI path each
peer is given its slot (`PSX_NET_SLOT`). R4 gives car *k* to session slot *k*
(`psx_netplay_seat_count()`, `psx_netplay_local_slot()`). The host must sit in
a seat; a host watching from the gallery is not supported.

**One race, every peer.** Each peer runs R4's native mode-4 link race with
the same inputs, so guest state is identical everywhere (the netplay digests
check it). Game-owned hooks in `src/mods/r4_link_netplay.c`, listed in
`game.toml`, replace the serial cable: the link setup and handshake are
satisfied without SIO1, the accepted-entrant flags mark the first N slots,
and each frame the published pad of every seat (`psx_netplay_sim_pad`) is
turned into R4's own command word for that seat's car. Pause and menu flags
come from any seat. These hooks need RetroPortingToolKit/psxrecomp#512.

**Views: the shared frame.** The original link console draws one view (two
entrants) or the two local players' views (four entrants). Online every peer
must draw the same frame, because drawing writes guest memory (ordering
tables, packets, camera state). So every peer's simulated frame draws all
seats' views: four 160x120 quadrants, seat 0 top-left, 1 top-right, 2
bottom-left, 3 bottom-right, each with its HUD. This frame is the canonical
one: guest RAM, VRAM, savestates, rollback snapshots and digests.

**Views: what each player sees.** Each peer then draws its *own* seat's view
full screen, 320x240 at the presenter's internal resolution, with the
full-screen HUD (rank, laps, time, tachometer, speed, gear, course map) and
the rear-view mirror, as the retail link console drew its one player, and
shows that instead of the shared frame. It is drawn at the main loop's
VSync(0) (0x8008B330, ra 0x8001E7E4), where the link handler has finished the
tick and the other buffer, the one the next flip shows, is free to redraw,
inside psxrecomp's sandboxed local view (`psx_mod_render_local_view`,
RetroPortingToolKit/psxrecomp#542). The sandbox
freezes guest time and restores the whole machine afterwards (CPU, RAM,
scratchpad, devices, the authoritative VRAM); only the OpenGL presenter's
surface keeps the image. The draw is the link handler's one-view path
(0x80115D80..0x80116254 with view flag 0, draw calls only): the seat's car
takes the P1 object's place, its lap time slot 0, its camera blocks the
view's camera; the one-view HUD layout and rank sprites are set up as race
init does for two entrants (0x80021014, 0x8002094C), and the rank total keeps
the real entrant count. The camera step runs once more on the already
stepped camera (the image leads by at most one smoothing step); the
countdown fade counter, engine audio and the packet send are left out. The
mirror's slide-in position is host state per peer (the shared frame never
draws a mirror).

Where the local view is unavailable (the software present: headless runs,
Vulkan, which netplay replaces with the software present; a refusal or a
rollback of the sandbox) a peer shows its own quadrant of the shared frame,
scaled at 4:3, through `psx_netplay_present_local_view`
(RetroPortingToolKit/psxrecomp#535); `PSX_R4_LINK_QUADRANT_VIEW=1` forces
that. Both are presentation only. Menus, Results and loading screens show
the whole frame. While paused every peer shows the whole frame: the two upper
views, the pause menu and a black lower half (the lower views' private OTs
would cover the menu, so they are not drawn while paused).

With two seats R4's entrant count is 2, which selects the one-view layout.
The hooks keep the count at 2 for the race, rank and results, take the
two-view branch of the link handler (0x801157EC) and let only the two-view
HUD setup (race init 0x8002094C) and HUD pass (0x80021134 from 0x80115F7C)
see the two-view count. The unused lower half is cleared black.

**Mode-4 frames without the race.** Results, Car Select and loading frames
are still mode 4. The OT and HUD edits only run in a frame whose views the
link race handler built (they once linked stale HUD copies into the Results
-> Car Select frame, which halted the GPU on an unknown GP0 command).

## The experimental switch stays

`PSX_R4_LINK_EXPERIMENTAL=1` is still required on every peer. The bridge
allocates its enhancement memory (Expansion 1 for brake ramps and two extra
cameras, the GPU DMA aperture for the lower views' ordering tables) when the
game starts, before anyone knows whether a link session will follow, and an
allocation makes that hardware region RAM instead of open bus. The default
path must stay hardware-faithful, so the allocation, and with it the bridge,
stays opt-in until the framework can reserve that memory for a netplay
session only (for example at session start, which already cold-boots the
game). The switch changes nothing for offline play or the stock VS Battle.

## Limits

- The own full-screen view needs the OpenGL presenter. With the software
  present (or Vulkan) a peer shows its 160x120 quadrant at 2x instead.
- Widescreen is not available online: psxrecomp clears the mod plan for
  every netplay match, so `r4.enhancement.widescreen` is off and the view is
  4:3.
- In the shared frame (and so in the quadrant fallback) the HUD of seats 3
  and 4 lacks the mph/rpm labels and its timer sits at the top edge of the
  quadrant.
- The own view costs one more draw of the race per tick on each peer
  (about 6-13 ms on an M-series Mac with four peers on one machine).
- Results name Player 1 and Player 2 only (the original link's two
  consoles); the cars of seats 3 and 4 appear without a name.
- In a two-player netplay session the main menu still offers VS Battle, which
  is the stock split screen on both peers (each sees both halves). Choose
  Link Battle.
- Physical controllers, remote networks and the GL window present (as
  opposed to the headless present-image ring) were not exercised by the
  harness below.

## Evidence

`tools/r4_online_battle_regression.py` boots N peers of one debug build
(`-DPSX_NETPLAY=ON`) on loopback LAN netplay, host = seat 0, guests started in
order, and drives every seat through the native menus with its own pad. A
test-only autopilot per seat holds accelerate and steers toward the course
centreline a speed-dependent distance ahead (it finds the course segment
table from the car's segment index, `car+0xF0`) and brakes into sharp bends.
It checks: entry with N accepted entrants; each seat in turn accelerates
alone from the grid and only its car moves; every peer presents its own view;
a race to the natural finish; Results; Car & Course Change into a second race;
pause from the last seat (every peer paused and showing the whole frame),
Retire from seat 0; Results; Exit to the title; matching digests on every
common tick (a tick a peer later rolled back over, i.e. one computed on a
predicted input, is left out); dispatch and segment misses.

`--frontend headless` (default) runs the software present, so each peer shows
its quadrant: its presented image (headless present ring) is compared with
the four quadrants of the same guest frame. `--frontend hidden` runs OpenGL
peers in never-shown windows, so each peer draws its own full-screen view:
its window image (`present_shot`, scaled to 160x120) is compared with the
middle band of each seat's quadrant of the canonical frame (`screenshot`, the
authoritative VRAM), and `render_pass_stats` counts its committed views.

Results at R4 `c34cf47` (psxrecomp `f379771c`), macOS arm64, one debug
build, rollback mode, 3-lap Helter Skelter, on a shared machine (load 12-20).

Own full-screen view (`--frontend hidden`):

| Peers | Own view (distance: own / other seats) | Own views drawn per peer (sandbox faults) | Natural finish | Car & Course Change | Pause (whole frame) / Retire / Exit | Digests | Dispatch / segment misses |
|---|---|---|---|---|---|---|---|
| 2 | 27.2 / 51.3; 33.0 / 51.3 | 12637 (0), 8.2-8.5 ms | 615 s, 23781 frames | new race, 2 cars | 9.8 vs 76-78; Retire; title | 973 common ticks, 0 mismatch | 0 / 0 |
| 3 | 28.0 / 34-40; 30.5 / 36-39; 43.6 / 60-62 | 12612 (0), 7.3-8.0 ms | 659 s, 24737 frames | new race, 3 cars | 9.5 vs 51-70; Retire; title | 1020, 0 mismatch | 0 / 0 |
| 4 | 27.9 / 34-45; 30.6 / 34-40; 39.5 / 48-61; 20.6 / 36-50 | 12500-12502 (0), 5.2-8.0 ms | 804 s, 31265 frames | new race, 4 cars | 9.6 vs 53-74; Retire; title | 1229, 0 mismatch (two rollbacks on mispredicted pads, one in race 2 with own views on, replayed and matched) | 0 / 0 |
| 2, Mac + Windows PC over the LAN | Mac 27.7 / 51.4; PC 43.8 / 47.8 | 12906 each (0); Mac 6.3 ms, PC 3.5 ms | 529 s, 24211 frames | new race, 2 cars | 9.5 vs 75-77; Retire; title | 993, 0 mismatch | 0 / 0 |

Quadrant fallback (`--frontend headless`, software present):

| Peers | Own quadrant (distance: own / others) | Natural finish | Car & Course Change | Pause / Retire / Exit | Digests | Misses |
|---|---|---|---|---|---|---|
| 2 | 2.2-2.4 / 41-43 | 348 s, 24265 frames | 2 cars | whole frame; Retire; title | 999, 0 mismatch | 0 / 0 |
| 3 | 0.0 / 34-55 | 429 s, 24770 frames | 3 cars | whole frame; Retire; title | 1025, 0 mismatch | 0 / 0 |
| 4 | 0.0-3.0 / 32-50 | 719 s, 31083 frames | 4 cars | whole frame; Retire; title | 1221, 0 mismatch | 0 / 0 |

The Mac/PC run used the same harness with the guest on a Windows PC
(MSYS2 MINGW64 build of the same commits, OpenGL in a hidden window, direct
LAN address, seat 1); both peers drew their own view from boot to the title.
In the own-view runs every window showed its own car full screen with its own
rank (n | N), laps, time limit, tachometer, speed and gear, course map and,
once it slides in, the rear-view mirror, while the canonical frame kept the
four quadrants. A four-peer run on a busy machine can still end with a
liveness timeout (an input stall over 1.5 s); none did here.

In every run each seat, accelerating alone from the grid, moved only its own
car (seats not yet pressed stayed put), and a race began with N accepted
entrants and N cars on every peer. The harness gives each headless peer its
slot (`PSX_NET_SLOT`, in launch order); the room's join-order seating
described above is the launcher's (`ae_np_lan_find_free_slot` and
`ae_np_plan_session_slots` in `psxrecomp/runtime/src/main.cpp`) and was not
re-run here, because that path opens (hidden) game windows.

`tools/r4_link_netplay_regression.py` (savestate loads and the earlier
three/four-seat probes) and `tools/r4_link_lan_launcher_regression.py`
(hidden-window LAN discovery, Join and seating through the launcher) remain
for those paths. `tools/r4_link_manifest.py` authenticates the US disc's EXE
and R4.BIN hook sites; `tests/test_r4_link_netplay.c` guards the hooks.

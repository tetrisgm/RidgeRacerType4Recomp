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

**Views.** The original link console draws one view (two entrants) or the two
local players' views (four entrants). Online every peer must draw the same
frame, so every peer draws all seats' views: four 160x120 quadrants, seat 0
top-left, 1 top-right, 2 bottom-left, 3 bottom-right, with each view's HUD
inside its quadrant. Each peer then presents only its own seat's quadrant,
scaled to the window at 4:3, through `psx_netplay_present_local_view`
(RetroPortingToolKit/psxrecomp#535). That request is presentation only: it
never reaches guest memory, savestates or digests. It lapses a few ticks after
the race handler stops renewing it, so menus, Results and loading screens show
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

- Each seat's view is 160x120 guest pixels, presented at 2x. Internal
  resolution 2x or higher gives a native-sharp picture; Native looks soft.
- The HUD of seats 3 and 4 lacks the mph/rpm labels and its timer sits at the
  top edge of the quadrant.
- Results name Player 1 and Player 2 only (the original link's two
  consoles); the cars of seats 3 and 4 appear without a name.
- In a two-player netplay session the main menu still offers VS Battle, which
  is the stock split screen on both peers (each sees both halves). Choose
  Link Battle.
- Physical controllers, remote networks and the GL window present (as
  opposed to the headless present-image ring) were not exercised by the
  harness below.

## Evidence

`tools/r4_online_battle_regression.py` boots N headless peers of one debug
build (`-DPSX_NETPLAY=ON`) on loopback LAN netplay, host = seat 0, guests
started in order, and drives every seat through the native menus with its own
pad. A test-only autopilot per seat holds accelerate and steers toward the
course centreline a speed-dependent distance ahead (it finds the course
segment table from the car's segment index, `car+0xF0`) and brakes into sharp
bends. It checks: entry with N accepted entrants; each seat in turn
accelerates alone from the grid and only its car moves; every peer presents
exactly its own quadrant (its presented image against the four quadrants of
the same guest frame); a race to the natural finish; Results; Car & Course
Change into a second race; pause from the last seat (every peer paused and
showing the whole frame), Retire from seat 0; Results; Exit to the title;
matching digests on every common tick; dispatch and segment misses.

Results at `d60f822` (psxrecomp `c4215396`), macOS arm64, one debug build,
rollback mode (the launcher's default), 3-lap Helter Skelter, on a shared
machine at load average 17-55:

| Peers | Seats (slot per peer) | Own view presented (quadrant distance: own / others) | Natural finish | Car & Course Change | Pause (whole frame) / Retire / Exit | Digests | Dispatch / segment misses |
|---|---|---|---|---|---|---|---|
| 2 | 0, 1 | 0.0 / 43-69 on both | 519 s, 24015 frames; Results: Player 2 won | new race, 2 cars | both paused and whole frame; Retire; Exit to title | 985 common ticks, 0 mismatch | 0 / 0 |
| 3 | 0, 1, 2 | 0.0-3.0 / 34-96 on all | 851 s, 25190 frames; Results: Player 1 won | new race, 3 cars | all three; Retire; Exit to title | 1032 common ticks, 0 mismatch | 0 / 0 |
| 4 | 0, 1, 2, 3 | 0.0-0.6 / 33-52 on all | 1525 s, 31391 frames; Results: Player 1 won | new race, 4 cars | all four; Retire; Exit to title | 1225 common ticks, 0 mismatch | 0 / 0 |

Three earlier four-peer runs (two rollback, one delay) were in sync (557-715
common ticks, 0 mismatch) until each ended at sim 18016-23121, late in the
race, with every peer reporting that the other player stopped responding: an
input stall longer than the framework's 1.5 s running liveness timeout while
the machine ran at load 20-55 from other work. The run in the table passed
under the same conditions; a busy host can still end a four-player match.

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

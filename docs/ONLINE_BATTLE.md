# Experimental Link Battle integration

The opt-in link path runs one R4 runtime per human seat. It uses R4's native
mode-4 four-car update loop and consumes synchronized netplay pad words in the
game's command buffers. It does not emulate the serial cable. The game-specific
hooks are statically linked in `src/mods/r4_link_netplay.c` and declared in
`game.toml`; regular mod entry hooks are cleared when netplay starts.

## Dependencies

The serial-free bridge needs framework APIs that are under review upstream:

- RetroPortingToolKit/psxrecomp#512 (`feat/link-seat-input`): game-owned
  netplay function hooks and filters (independent of the mod plan a match
  clears), the published seat pads and seat count, and the unconfirmed
  load barrier in both netplay modes;
- RetroPortingToolKit/psxrecomp#511: no duplicate restore in a mixed-hash
  LOAD (needed when peers hold different checkpoints);
- RetroPortingToolKit/recomp-net#26: matching seats wait for the host's
  release of a SAVE hash probe (three or more seats);
- RetroPortingToolKit/recomp-ui#80: the LAN page lists
  local rooms without connecting to the online lobby.

This branch pins psxrecomp and recomp-ui to the first and last of those. `[controller] multitap = false` (from #512) keeps offline play at two standalone pads; `players = 4` only sets the netplay seat count.
Regenerate the game after changing the framework pin; never edit
`generated/` directly. Run `tools/launch_link_experimental.sh BUILD_DIR` to
open the normal graphical launcher with R4's two experimental environment
flags enabled. Each peer selects the same player count. For a three-seat
session, R4 runs three active cars; the fourth slot is inactive.

## Evidence and limits

`tools/r4_link_manifest.py` authenticates the US disc's EXE and R4.BIN hook
sites. `tools/r4_link_state_probe.py` proves four independent command words
reach four native car updates in one runtime. The native test
`tests/test_r4_link_netplay.c` guards the game hooks, including zero-handle
serial event teardown. The loopback driver
`tools/r4_link_netplay_regression.py` loads separate peers and compares common
guest-state digests. Its disposable result files are under `/tmp`; they are
not part of the repository. The private graphical launcher regression
`tools/r4_link_lan_launcher_regression.py` exercises LAN discovery, Join,
host Play, and optional loading of a compatible race checkpoint.

Three-peer headless play completed a native two-lap Helter Skelter race, showed
Results, then used native Car & Course Change to select cars and enter a new
two-lap race with matching peer digests. Four-peer headless play completed both
laps with four active cars and four rendered views, then reached native Results.
Ordinary result-menu inputs selected Car & Course Change and restarted all four
peers in a new mode-4 race; 144 common guest-state digest checkpoints matched.
The graphical LAN page now lists local rooms without connecting to the online
lobby server. A second instance on this Mac could discover the host room but
could not reach the host's advertised NIC address by self-directed UDP; it
could reach the same host on loopback. Join verifies a matching local room
registry before using loopback, then uses UDP membership so every guest gets
the host's START, BIOS, timing, and rollback contract. The file-only local
seating path was removed because it could not deliver that contract.
The graphical regression passed with three peers seated 3/3 and all three
game runtimes armed. A checkpoint-free run then entered native Link Battle
through the normal R4 menus and reached mode 4, race phase 2 with three active
cars on every peer. A synchronized slot 06 checkpoint from that race reloaded
successfully in a separate graphical LAN session. Four peers also seated 4/4,
armed all four runtimes, and loaded a compatible synchronized checkpoint into
mode 4, race phase 2 with four active cars on each peer.

For fresh three-seat entry, `tools/r4_link_lan_launcher_regression.py`
supports `--peers 3 --fresh-race`. After game frame 1200, it pulses Start
(`0xFFF7` for eight frames, then neutral `0xFFFF` for at least 72) on all
active peers until mode at `0x800F4EF4` is 4, the link FSM halfword at
`0x800FF838` is 2, and accepted-seat bytes at `0x800AC074..77` are
`01 01 01 00` on every peer. In the observed run, five pulses sufficed.
After the Link Battle title settles into Preset Player 1 Car Select, four
Circle pulses (`0xDFFF`, eight frames plus at least 80 neutral frames) on
all active peers advance the two preset-car selections; the next screen is
native Course Select with Start highlighted. One more Circle pulse starts
the race. The success condition is mode 4, active-car count 3 at
`0x800AC754`, and race phase 2 at `0x800FF860` on every peer. The regression
captures Car Select, Course Select, and race screenshots. Its optional
`--checkpoint-slot` saves a new synchronized race state only to an unused
slot in the runtime's supported 0..11 range.

The authenticated retail link overlay reads Start from `0x800F3BF0`, sends a
pause flag in serial packet byte 3, and ORs the received/sent flags in its pause
handler at `0x80115B58`-`0x80115B88`. The serial-free hook now supplies one
flag pulse for each newly pressed synchronized Start seat. The held-seat mask
lives in authenticated unused guest byte `0x801190BE`, preserving the existing
enhancement-memory layout and rollback behavior. A four-peer headless test
pressed Start from seat 4: all pause bytes became 1 and R4's native pause menu
rendered. Car positions stayed fixed for 64 host frames; a second Start resumed
all peers, car positions changed, and post-resume digests matched.

In the three-peer Results menu, selecting Exit returned all peers to the R4
title screen with matching digests. The launcher's scripted Quit also exited
cleanly. A visible host Escape and a visible seat-4 Escape each sent a netplay
BYE: all four processes exited normally within one second, with the leaving
peer reporting `netplay_escape` and the others `netplay_peer_disconnect`.
An abrupt debug quit still provides a bounded fallback: the remaining peers
waited for confirmation, then exited after the runtime's 18-second stall
timeout. The direct CLI tests do not verify the launcher's post-disconnect UI.

## What is and is not claimed

Verified (headless loopback peers, earlier framework pins; per-peer digests
matched throughout): three peers through a native two-lap race, Results,
Car & Course Change restart and title Exit; four peers with four accepted
seats and rendered views, synchronized pause/resume from any seat, native
Retire to Results, restart, and BYE on host or guest Escape; synchronized
save/load, including a mixed-hash LOAD with psxrecomp#511.

Not claimed: a natural four-player race from the start line to the finish.
Four-peer finishes so far ran from a late-race checkpoint or ended through
Retire; two capped runs with the minimap driver did not reach the finish in
900 s. Physical controllers and remote (non-loopback) networks are untested.

The runtime requires matching binaries and disc identity on every peer. The
serial-free hooks are gated by `PSX_R4_LINK_EXPERIMENTAL=1` and a live 3/4-seat
netplay session. Regular two-player VS Battle remains the stock path.

#!/usr/bin/env python3
"""Exercise the private LAN launcher discovery, Join, Play, and optional race.

Use a disposable build and peer directories. Every peer uses the same runtime
executable, while each guest has its own game.toml, bios/, and memcards/. The
optional race slot must have been saved by this exact generated build and be
present in the host openbios/ and each guest netplay/ memcard directory.
"""

import argparse
import json
import os
import re
import subprocess
import time
from pathlib import Path

from dbg import cmd


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", required=True, type=Path)
    parser.add_argument("--peer-prefix", required=True, type=Path)
    parser.add_argument("--disc", required=True, type=Path)
    parser.add_argument("--peers", required=True, type=int, choices=(2, 3, 4))
    race = parser.add_mutually_exclusive_group()
    race.add_argument("--race-slot", type=int,
                      help="load a matching synchronized race checkpoint")
    race.add_argument("--fresh-race", action="store_true",
                      help="enter Link Battle from boot with normal pad inputs")
    parser.add_argument("--checkpoint-slot", type=int,
                        help="save a new synchronized checkpoint after fresh race entry (0..11)")
    parser.add_argument("--debug-base", type=int, default=6895)
    parser.add_argument("--exe-name", default="r4-runtime",
                        help="runtime file in --build (a hard link under another "
                             "name keeps a machine-wide `pkill r4-runtime` away)")
    parser.add_argument("--report", required=True, type=Path)
    args = parser.parse_args()
    if args.checkpoint_slot is not None and not args.fresh_race:
        parser.error("--checkpoint-slot requires --fresh-race")
    if args.checkpoint_slot is not None and not 0 <= args.checkpoint_slot <= 11:
        parser.error("--checkpoint-slot must be 0..11")
    build = args.build.resolve()
    runtime = build / args.exe_name
    room = build / "netplay_lan_lobby.txt"
    if not runtime.is_file() or not args.disc.is_file():
        parser.error("runtime or disc is missing")
    if room.exists():
        parser.error(f"LAN registry already exists: {room}")
    directories = [build] + [Path(f"{args.peer_prefix}{i}").resolve()
                             for i in range(1, args.peers)]
    for directory in directories:
        if not (directory / "game.toml").is_file() or not (
                directory / "bios/openbios.bin").is_file():
            parser.error(f"game.toml or OpenBIOS missing in {directory}")
    if args.checkpoint_slot is not None:
        for i, directory in enumerate(directories):
            subdir = "openbios" if i == 0 else "netplay"
            path = directory / "memcards" / subdir / (
                f"state_8007D8F4_slot{args.checkpoint_slot:02d}.pst")
            if path.exists():
                parser.error(f"refusing to replace existing checkpoint: {path}")
    args.report.parent.mkdir(parents=True, exist_ok=True)
    processes = []
    handles = []

    def log_path(i):
        return args.report.with_name(f"{args.report.stem}.peer{i}.log")

    def wait(predicate, timeout, label):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if predicate():
                return
            if any(process.poll() is not None for process in processes):
                raise RuntimeError(f"runtime exited while waiting for {label}")
            time.sleep(0.1)
        raise TimeoutError(label)

    def launch(i, script):
        env = os.environ.copy()
        env.update(LNG_TEST_HIDDEN="1", LNG_SCRIPT=script, SDL_AUDIODRIVER="dummy",
                   PSX_R4_LINK_EXPERIMENTAL="1")
        handle = log_path(i).open("w")
        handles.append(handle)
        processes.append(subprocess.Popen(
            [str(runtime), "--game", "./game.toml", "--disc", str(args.disc),
             "--bios", "./bios/openbios.bin", "--memcard-dir",
             str(directories[i] / "memcards"), "--debug-port",
             str(args.debug_base + i), "--launcher"],
            cwd=directories[i], env=env, stdout=handle,
            stderr=subprocess.STDOUT))

    def seat_ids():
        if not room.is_file():
            return 0, []
        rows = room.read_text().splitlines()
        capacity = int(rows[9])
        return capacity, rows[10 + capacity:10 + 2 * capacity]

    def seats():
        capacity, ids = seat_ids()
        return capacity, sum(bool(row) for row in ids)

    def ask(i, name, **fields):
        return cmd(dict(cmd=name, **fields), port=args.debug_base + i, timeout=3)

    def read_int(i, address, size):
        response = ask(i, "read_ram", addr=hex(address), len=size)
        return int.from_bytes(bytes.fromhex(response["hex"]), "little")

    def hold_input(buttons, held=8, released=72):
        start = ask(0, "frame")["frame"]
        for i in range(args.peers):
            ask(i, "set_input", buttons=hex(buttons))
        wait(lambda: ask(0, "frame")["frame"] >= start + held, 15,
             "pad hold")
        for i in range(args.peers):
            ask(i, "set_input", buttons="0xFFFF")
        wait(lambda: ask(0, "frame")["frame"] >= start + held + released,
             20, "pad release")

    def race_ready():
        return all(read_int(i, 0x800F4EF4, 2) == 4 and
                   read_int(i, 0x800AC754, 2) == args.peers and
                   read_int(i, 0x800FF860, 4) == 2
                   for i in range(args.peers))

    try:
        # Hidden launcher scripts use the fixed 1100x880 logical layout.
        # For three seats select the third entry in Max Players first.
        select_three = "click:400,406;wait:5;click:355,483;wait:5;" if args.peers == 3 else ""
        host_script = ("view:netplay_mode;wait:5;click:100,275;wait:10;"
                       "click:120,812;wait:15;" + select_three +
                       "click:540,608;wait:750;click:965,815;wait:1000")
        # Two players: keep the default 4-seat room and start it with two
        # seated (a room starts with any 2+ seated players).
        capacity_expected = 4 if args.peers == 2 else args.peers
        launch(0, host_script)
        wait(lambda: room.is_file(), 12, "host lobby")
        capacity, seated = seats()
        if (capacity, seated) != (capacity_expected, 1):
            raise AssertionError(f"host lobby seating {(capacity, seated)}")
        join_order = [[k for k, row in enumerate(seat_ids()[1]) if row]]
        for i in range(1, args.peers):
            launch(i, "view:netplay_mode;wait:5;click:100,275;wait:20;"
                      "click:1035,198;wait:2000")
            wait(lambda: seats() == (capacity_expected, i + 1), 10,
                 f"guest {i} discovery and Join")
            join_order.append([k for k, row in enumerate(seat_ids()[1]) if row])
        wait(lambda: all("netplay lockstep armed" in
                         log_path(i).read_text(errors="replace")
                         for i in range(args.peers)), 60, "host Play and guest START")
        for i in range(args.peers):
            if "login_required" in log_path(i).read_text(errors="replace"):
                raise AssertionError(f"LAN launcher {i} attempted lobby login")
        # Lobby seat each newcomer took, and the session slot each game
        # runtime then reports.
        lobby_seat = [sorted(set(after) - set(before))
                      for before, after in zip([[]] + join_order, join_order)]
        game_slot = []
        for i in range(args.peers):
            match = re.search(r"psx_netplay: started transport=\S+ slot=(\d+)",
                              log_path(i).read_text(errors="replace"))
            game_slot.append(int(match.group(1)) if match else None)
        result = dict(peers=args.peers, capacity=capacity_expected,
                      seated=seats()[1], lobby_seat_by_join_order=lobby_seat,
                      session_slot_by_join_order=game_slot,
                      all_game_runtimes_armed=True,
                      logs=[str(log_path(i)) for i in range(args.peers)])
        if args.fresh_race:
            wait(lambda: ask(0, "frame").get("frame", 0) >= 1200, 90,
                 "normal boot input window")
            # R4's mode-0 opening can still be showing FMV. Repeated, bounded
            # Start edges admit the active seats when native Link Entry opens.
            # 0x800AC074..77 are the accepted-seat flags; the unused fourth
            # remains zero in a three-peer session.
            def accepted():
                return all(read_int(i, 0x800F4EF4, 2) == 4 and
                           read_int(i, 0x800FF838, 2) == 2 and
                           bytes.fromhex(ask(i, "read_ram", addr="0x800AC074",
                                                 len=4)["hex"]) ==
                           bytes([1] * args.peers + [0] * (4 - args.peers))
                           for i in range(args.peers))
            start_pulses = 0
            for _ in range(75):
                if accepted():
                    break
                hold_input(0xFFF7)
                start_pulses += 1
            if not accepted():
                raise TimeoutError("three/four native Link Entry admissions")
            result["link_entry"] = dict(start_pulses=start_pulses,
                                        fsm=2,
                                        accepted=[1] * args.peers +
                                                 [0] * (4 - args.peers))
            # After the Link Battle title, native Preset Player 1 and 2 each
            # advance through two Circle confirmations. The active peers
            # receive each pulse, then the highlighted Course Select Start
            # accepts one final Circle.
            start = ask(0, "frame")["frame"]
            wait(lambda: ask(0, "frame")["frame"] >= start + 600,
                 30, "Car Select transition")
            ask(0, "screenshot", path=str(args.report.with_suffix(".car-select.png")))
            for _ in range(4):
                hold_input(0xDFFF, released=80)
            ask(0, "screenshot", path=str(args.report.with_suffix(".course-select.png")))
            hold_input(0xDFFF, released=80)
            wait(race_ready, 90, "fresh mode-4 race")
            ask(0, "screenshot", path=str(args.report.with_suffix(".race.png")))
            result["race"] = dict(mode=4, cars=args.peers, phase=2,
                                  source="normal_menus")
            if args.checkpoint_slot is not None:
                generation = [ask(i, "savestate_status")["generation"]
                              for i in range(args.peers)]
                response = ask(0, "savestate", op="save", slot=args.checkpoint_slot)
                if not response.get("ok"):
                    raise RuntimeError(f"checkpoint save refused: {response}")
                wait(lambda: all(
                    (status := ask(i, "savestate_status"))["generation"] > generation[i]
                    and status["last_ok"] and status["last_slot"] == args.checkpoint_slot
                    for i in range(args.peers)), 30, "synchronized checkpoint save")
                result["checkpoint_slot"] = args.checkpoint_slot
        if args.race_slot is not None:
            wait(lambda: all(ask(i, "frame").get("frame", 0) > 120
                             for i in range(args.peers)), 20, "game frames")
            generation = [ask(i, "savestate_status")["generation"]
                          for i in range(args.peers)]
            response = ask(0, "savestate", op="load", slot=args.race_slot)
            if not response.get("ok"):
                raise RuntimeError(f"race load request refused: {response}")
            wait(lambda: all(
                (status := ask(i, "savestate_status"))["generation"] > generation[i]
                and status["last_ok"] for i in range(args.peers)), 30,
                "matching race checkpoint load")
            wait(race_ready, 20, "mode-4 race")
            result["race"] = dict(mode=4, cars=args.peers, phase=2,
                                  slot=args.race_slot)
        args.report.write_text(json.dumps(result, indent=2) + "\n")
        print(json.dumps(result), flush=True)
    finally:
        for process in processes:
            if process.poll() is None:
                process.terminate()
        for process in processes:
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        for handle in handles:
            handle.close()
        room.unlink(missing_ok=True)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Local headless regression for synchronized R4 savestate loads.

Requires preprovisioned disposable peer directories named PREFIX0, PREFIX1,
etc. Each contains r4-runtime, game.toml, bios/, mods/, and memcards/ with
the selected compatible save. The disc and save remain local and untracked.
"""

import argparse
import json
import math
import os
import re
import signal
import struct
import subprocess
import time
from pathlib import Path

from dbg import cmd


def view_draw_counts(frames, seats):
    """Count GP0 draw primitives under each seat's quadrant draw area."""
    expected = [(0, 0, 159, 119), (160, 0, 319, 119),
                (0, 120, 159, 239), (160, 120, 319, 239)][:seats]
    best = [0] * len(expected)
    for frame in frames:
        counts = [0] * len(expected)
        left_top = right_bottom = None
        for entry in frame.get("entries", []):
            op = int(entry["op"], 16)
            if not entry.get("w"):
                continue
            word = int(entry["w"][0], 16)
            if op in (0xE3, 0xE4):
                point = (word & 1023, (word >> 10) & 511)
                if op == 0xE3:
                    left_top = point
                else:
                    right_bottom = point
            elif 0x20 <= op < 0x80 and left_top and right_bottom:
                y_base = (left_top[1] // 240) * 240
                clip = (left_top[0], left_top[1] - y_base,
                        right_bottom[0], right_bottom[1] - y_base)
                if clip in expected:
                    counts[expected.index(clip)] += 1
        best = [max(a, b) for a, b in zip(best, counts)]
    return dict(zip(("top_left", "top_right", "lower_left", "lower_right"),
                    best))


def view_speed_digits(frame, seats):
    """Decode R4's native 12-pixel speed glyphs from each split viewport."""
    anchors = [(92, 80), (252, 80), (92, 200), (252, 200)][:seats]
    glyphs = {}
    for entry in frame.get("entries", []):
        if entry.get("op") != "0x65" or len(entry.get("w", [])) < 4:
            continue
        words = [int(value, 16) for value in entry["w"][:4]]
        x, y = words[1] & 0xFFFF, words[1] >> 16
        u, v = words[2] & 0xFF, (words[2] >> 8) & 0xFF
        if (v == 240 and u <= 108 and u % 12 == 0 and
                words[3] == 0x0010000C):
            glyphs[x, y] = u // 12
    values = []
    for x, y in anchors:
        digits = [glyphs.get((x + 8 * digit, y)) for digit in range(3)]
        values.append(None if any(digit is None for digit in digits) else
                      digits[0] * 100 + digits[1] * 10 + digits[2])
    return values


def view_gear_digits(frame, seats):
    """Decode R4's native single-glyph gear display in each viewport."""
    anchors = [(136, 66), (290, 66), (136, 186), (290, 186)][:seats]
    glyphs = {}
    for entry in frame.get("entries", []):
        if entry.get("op") != "0x65" or len(entry.get("w", [])) < 4:
            continue
        words = [int(value, 16) for value in entry["w"][:4]]
        x, y = words[1] & 0xFFFF, words[1] >> 16
        u, v = words[2] & 0xFF, (words[2] >> 8) & 0xFF
        if (v == 240 and 120 <= u <= 216 and (u - 120) % 12 == 0 and
                words[3] == 0x0010000C):
            glyphs[x, y] = (u - 120) // 12
    return [glyphs.get(anchor) for anchor in anchors]


def top_view_lap_digits(frame):
    """Read the native LAPS current/total glyphs in the upper viewport."""
    glyphs = {}
    for entry in frame.get("entries", []):
        if entry.get("op") != "0x65" or len(entry.get("w", [])) < 4:
            continue
        words = [int(value, 16) for value in entry["w"][:4]]
        x, y = words[1] & 0xFFFF, words[1] >> 16
        u, v = words[2] & 0xFF, (words[2] >> 8) & 0xFF
        if ((x, y) in ((280, 12), (300, 12)) and v == 168 and
                u <= 108 and u % 12 == 0 and words[3] == 0x000C000C):
            glyphs[x] = u // 12
    return [glyphs.get(280), glyphs.get(300)]


def minimap_marker(screenshot, previous=None, start_segment=0):
    """Locate R4's orange P1 marker in the upper view's native 320x240 map."""
    rgb = subprocess.check_output([
        "ffmpeg", "-v", "error", "-i", str(screenshot),
        "-f", "rawvideo", "-pix_fmt", "rgb24", "-"])
    if len(rgb) != 320 * 240 * 3:
        raise RuntimeError("minimap driver needs a 320x240 screenshot")
    points = set()
    for y in range(68, 104):
        for x in range(14, 64):
            offset = 3 * (320 * y + x)
            r, g, b = rgb[offset:offset + 3]
            if r > 100 and r > 1.4 * g and r > 1.4 * b:
                points.add((x, y))
    markers = []
    while points:
        component = {points.pop()}
        pending = list(component)
        while pending:
            x, y = pending.pop()
            for neighbor in ((x - 1, y), (x + 1, y),
                             (x, y - 1), (x, y + 1)):
                if neighbor in points:
                    points.remove(neighbor)
                    component.add(neighbor)
                    pending.append(neighbor)
        if len(component) == 16:
            xs, ys = zip(*component)
            if max(xs) - min(xs) == 3 and max(ys) - min(ys) == 3:
                markers.append((min(xs) + 1.5, min(ys) + 1.5))
    reference = previous if previous is not None else R4_HELTER_MAP_PATH[start_segment]
    if not markers:
        raise RuntimeError("orange 4x4 minimap marker is absent")
    nearest = min(markers, key=lambda marker: math.dist(marker, reference))
    if math.dist(nearest, reference) > (8 if previous is not None else 12):
        raise RuntimeError(f"minimap marker jumped from {reference} to {markers}")
    return nearest


R4_HELTER_MAP_PATH = (
    (42, 100), (23, 101), (18, 97), (18, 77), (18, 72),
    (23, 72), (33, 77), (39, 77), (44, 75), (47, 70),
    (50, 70), (53, 75), (53, 79), (49, 83), (49, 98),
    (58, 98), (58, 100), (46, 100), (42, 100))


def minimap_steering(marker, yaw, segment):
    """Steer toward a point five map pixels ahead on the drawn course line."""
    path = R4_HELTER_MAP_PATH
    if segment >= len(path) - 3 and math.dist(marker, path[0]) < 4:
        segment = 0
    candidates = range(max(0, segment - 2), min(len(path) - 1, segment + 3))
    def project(index):
        ax, ay = path[index]
        bx, by = path[index + 1]
        dx, dy = bx - ax, by - ay
        t = max(0, min(1, ((marker[0] - ax) * dx + (marker[1] - ay) * dy) /
                       (dx * dx + dy * dy)))
        x, y = ax + t * dx, ay + t * dy
        return (marker[0] - x) ** 2 + (marker[1] - y) ** 2, t
    segment = min(candidates, key=lambda index: project(index)[0])
    _, t = project(segment)
    lookahead = 3.0 if segment in (2, 3) else (1.0 if segment in (14, 15) else
                                             (2.0 if segment == 13 else 5.0))
    target = path[-1]
    for index in range(segment, len(path) - 1):
        ax, ay = path[index]
        bx, by = path[index + 1]
        start = t if index == segment else 0.0
        remaining = (1 - start) * math.dist(path[index], path[index + 1])
        if lookahead <= remaining:
            fraction = start + lookahead / math.dist(path[index], path[index + 1])
            target = (ax + fraction * (bx - ax), ay + fraction * (by - ay))
            break
        lookahead -= remaining
    dx, dy = target[0] - marker[0], target[1] - marker[1]
    desired = (math.atan2(dx, -dy) * 4096 / (2 * math.pi)) % 4096
    error = (desired - yaw + 2048) % 4096 - 2048
    steering = 0 if abs(error) < 96 else (1 if error < 0 else 2)
    return segment, steering, round(error), round(desired)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--peer-prefix", required=True, type=Path)
    p.add_argument("--disc", required=True, type=Path)
    p.add_argument("--debug-port-base", type=int, default=4895)
    p.add_argument("--udp-port-base", type=int, default=47841)
    p.add_argument("--peers", required=True, type=int, choices=(2, 3, 4))
    p.add_argument("--slot", required=True, type=int)
    p.add_argument("--mode", choices=("rollback", "delay"), default="rollback")
    p.add_argument("--visible-host", action="store_true",
                   help="open peer 0's game window; other peers stay headless")
    p.add_argument("--visible-peer", type=int,
                   help="open one peer's game window; other peers stay headless")
    p.add_argument("--escape-probe", action="store_true",
                   help="macOS: send Escape to the visible host and verify BYE exit")
    p.add_argument("--fresh-link", action="store_true")
    p.add_argument("--resume-race", action="store_true",
                   help="load a saved Link Battle race and skip Course Select")
    p.add_argument("--menu-buttons", default="",
                   help="comma-separated active-low host taps before accepting Course Select")
    p.add_argument("--menu-screenshot", type=Path,
                   help="capture Course Select after --menu-buttons")
    p.add_argument("--menu-screenshot-each", action="store_true",
                   help="also capture Course Select after each scripted menu tap")
    p.add_argument("--menu-only", action="store_true",
                   help="stop after Course Select menu probe")
    p.add_argument("--menu-gap-frames", type=int, default=20,
                   help="frames between scripted menu taps")
    p.add_argument("--through-sim", type=int, default=192)
    p.add_argument("--race-telemetry-interval", type=int, default=0,
                   help="sample car X/Z, yaw, a progress candidate and speed every N race sim ticks")
    p.add_argument("--minimap-driver", action="store_true",
                   help="test-only Helter Skelter P1 driver using rendered minimap")
    p.add_argument("--driver-start-segment", type=int, default=0,
                   help="route segment for the first marker after a race checkpoint load")
    p.add_argument("--driver-recovery", action="store_true",
                   help="try bounded steering recovery when P1 stops on the route")
    p.add_argument("--driver-hook-east", action="store_true",
                   help="diagnostic: hold an eastward heading at the right hook")
    p.add_argument("--driver-hook-east-south", action="store_true",
                   help="diagnostic: clear the hook eastward, then turn south and west")
    p.add_argument("--driver-top-straight-heading", type=int,
                   help="hold a fixed yaw target while P1 traverses the top straight")
    p.add_argument("--driver-top-straight-band", type=int, default=48,
                   help="yaw tolerance before steering toward the top-straight target")
    p.add_argument("--save-checkpoint-slot", type=int,
                   help="synchronize a netplay save after reaching --through-sim")
    p.add_argument("--stop-on-phase-change", action="store_true",
                   help="capture race exit when the native race phase leaves 2")
    p.add_argument("--phase-settle-sim", type=int, default=0,
                   help="sim ticks to wait after race phase exit before capturing results")
    p.add_argument("--result-buttons", default="",
                   help="comma-separated active-low host taps after a settled result screen")
    p.add_argument("--sim-timeout", type=int, default=50,
                   help="seconds allowed to reach --through-sim")
    p.add_argument("--race-buttons", default="0xBFFF,0xBF7F,0xBFDF,0xBFFF",
                   help="comma-separated raw active-low pad words for race seats")
    p.add_argument("--repeat-loads", type=int, default=1)
    p.add_argument("--delay-guest-ms", type=int, default=0,
                   help="pause guest before each repeated load request")
    p.add_argument("--delay-host-after-guest-apply-ms", type=int, default=0,
                   help="pause host after a guest starts applying the first load")
    p.add_argument("--report", type=Path, required=True)
    p.add_argument("--gpu-report", type=Path,
                   help="capture recent headless GP0 draw commands for view checks")
    p.add_argument("--screenshot", type=Path,
                   help="capture peer 0's presented frame before stopping peers")
    p.add_argument("--pause-screenshot", type=Path,
                   help="press Start from seat 1 and capture the pause state")
    p.add_argument("--resume-screenshot", type=Path,
                   help="press Start again and capture the resumed state")
    p.add_argument("--pause-all", action="store_true",
                   help="press Start on every peer for the pause probe")
    p.add_argument("--pause-seat", type=int, default=0,
                   help="seat whose Start button triggers the pause probe")
    p.add_argument("--pause-menu-probe", action="store_true",
                   help="move the native pause cursor Down from one seat and Up from another")
    p.add_argument("--hud-write-trace", type=Path,
                   help="capture native HUD packet writes and adjacent guest state")
    p.add_argument("--event-write-trace", type=Path,
                   help="capture writes to OpenBIOS EvCB slot zero during result transition")
    p.add_argument("--hud-packet-dump", type=Path,
                   help="capture fixed retail HUD packet tail for analysis")
    p.add_argument("--race-timeout", type=int, default=50,
                   help="seconds allowed for fresh Link Battle entry")
    args = p.parse_args()
    if args.resume_race and not args.fresh_link:
        p.error("--resume-race requires --fresh-link")
    if args.visible_peer is not None and not 0 <= args.visible_peer < args.peers:
        p.error("--visible-peer must name an active peer")
    if args.escape_probe and not (args.visible_host or args.visible_peer is not None):
        p.error("--escape-probe requires a visible peer")
    if args.resume_race and (args.menu_buttons or args.menu_screenshot or args.menu_only):
        p.error("a resumed race cannot use Course Select probes")
    if not 0 <= args.driver_start_segment < len(R4_HELTER_MAP_PATH) - 1:
        p.error("--driver-start-segment is outside the course route")
    if args.save_checkpoint_slot is not None and not args.fresh_link:
        p.error("--save-checkpoint-slot requires --fresh-link")
    if args.stop_on_phase_change and not args.fresh_link:
        p.error("--stop-on-phase-change requires --fresh-link")
    if args.phase_settle_sim < 0 or (args.phase_settle_sim and
                                    not args.stop_on_phase_change):
        p.error("--phase-settle-sim requires --stop-on-phase-change")
    if args.result_buttons and not args.stop_on_phase_change:
        p.error("--result-buttons requires --stop-on-phase-change")
    if (args.menu_buttons or args.menu_screenshot or args.menu_only) and not args.fresh_link:
        p.error("menu probes require --fresh-link")
    if args.menu_screenshot_each and not args.menu_screenshot:
        p.error("--menu-screenshot-each requires --menu-screenshot")
    if args.menu_gap_frames < 20 or args.menu_gap_frames > 120:
        p.error("--menu-gap-frames must be between 20 and 120")
    if args.race_telemetry_interval < 0:
        p.error("--race-telemetry-interval must be nonnegative")
    if args.minimap_driver:
        if not args.fresh_link:
            p.error("--minimap-driver requires --fresh-link")
        if not args.race_telemetry_interval:
            args.race_telemetry_interval = 32
    if args.resume_screenshot and not args.pause_screenshot:
        p.error("--resume-screenshot requires --pause-screenshot")
    if args.pause_menu_probe and (not args.pause_screenshot or
                                  not args.resume_screenshot):
        p.error("--pause-menu-probe requires pause and resume screenshots")
    if args.pause_seat < 0 or args.pause_seat >= args.peers:
        p.error("--pause-seat must name an active peer")
    try:
        race_buttons = [int(value, 0) for value in args.race_buttons.split(",")]
    except ValueError:
        p.error("--race-buttons needs comma-separated integer pad words")
    if len(race_buttons) != 4 or any(not 0 <= value <= 0xFFFF
                                     for value in race_buttons):
        p.error("--race-buttons needs exactly four 16-bit pad words")
    try:
        menu_buttons = [int(value, 0) for value in args.menu_buttons.split(",")
                        if value]
    except ValueError:
        p.error("--menu-buttons needs comma-separated integer pad words")
    if any(not 0 <= value <= 0xFFFF for value in menu_buttons):
        p.error("--menu-buttons requires 16-bit pad words")
    try:
        result_buttons = [int(value, 0) for value in args.result_buttons.split(",")
                          if value]
    except ValueError:
        p.error("--result-buttons needs comma-separated integer pad words")
    if any(not 0 <= value <= 0xFFFF for value in result_buttons):
        p.error("--result-buttons requires 16-bit pad words")
    if args.repeat_loads < 1 or (args.repeat_loads != 1 and
                                 (args.fresh_link or args.peers != 2)):
        p.error("repeated loads require an ordinary two-peer save")
    if args.delay_guest_ms < 0 or args.delay_guest_ms > 2000:
        p.error("guest delay must be between 0 and 2000 ms")
    if not 0 <= args.delay_host_after_guest_apply_ms <= 2000:
        p.error("host delay must be between 0 and 2000 ms")
    prefix = str(args.peer_prefix)
    base_debug = args.debug_port_base
    base_udp = args.udp_port_base
    visible_peer = args.visible_peer if args.visible_peer is not None else (
        0 if args.visible_host else -1)
    logs = [Path(f"{args.report}.peer{i}.log") for i in range(args.peers)]
    procs = []
    handles = []

    def ask(i, name, **fields):
        return cmd(dict(cmd=name, **fields), port=base_debug + i, timeout=3)

    def read(i, address, length):
        return bytes.fromhex(ask(i, "read_ram", addr=hex(address), len=length)["hex"])

    def log_text(i):
        return logs[i].read_text(errors="replace")

    def digests(i):
        return {int(tick): core for tick, core in re.findall(
            r"rb live dig local sim=(\d+) core=([0-9a-f]+)", log_text(i))}

    def wait_until(predicate, seconds, label):
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            if any(proc.poll() is not None for proc in procs):
                raise RuntimeError(f"peer exited while waiting for {label}")
            if predicate():
                return
            time.sleep(0.05)
        raise TimeoutError(label)

    try:
        for i in range(args.peers):
            directory = Path(prefix + str(i))
            env = os.environ.copy()
            env.update(PSX_NETPLAY="1", PSX_NET_MODE=args.mode,
                       PSX_NET_TRANSPORT="lan", PSX_NET_SLOTS=str(args.peers),
                       PSX_NET_SLOT=str(i), PSX_NET_BIND=f"127.0.0.1:{base_udp+i}",
                       PSX_NET_SESSION_ID="98434")
            if args.fresh_link:
                env.update(PSX_R4_LINK_EXPERIMENTAL="1",
                           PSX_NETPLAY_HEADLESS_INPUT_PROBE="1")
            else:
                env.pop("PSX_R4_LINK_EXPERIMENTAL", None)
            if i:
                env["PSX_NET_PEER"] = f"127.0.0.1:{base_udp}"
            else:
                env.pop("PSX_NET_PEER", None)
            handles.append(logs[i].open("w"))
            command = ["./r4-runtime", "--game", "./game.toml", "--bios",
                       "./bios/openbios.bin", "--disc", str(args.disc),
                       "--memcard-dir", "./memcards", "--debug-port",
                       str(base_debug+i), "--no-launcher"]
            if i != visible_peer:
                command.append("--headless")
            procs.append(subprocess.Popen(
                command,
                cwd=directory, env=env, stdout=handles[-1], stderr=subprocess.STDOUT))
        wait_until(lambda: all("netplay lockstep armed" in log_text(i)
                               for i in range(args.peers)), 20, "netplay arm")
        if args.hud_write_trace:
            ask(0, "wtrace_range", lo="0x800F2AA4", hi="0x800F2B90")
            ask(0, "wtrace_add", lo="0x800D1F00", hi="0x800D2300")
            ask(0, "wtrace_add", lo="0x800F2B90", hi="0x800F4000")
        if args.event_write_trace:
            ask(0, "wtrace_range", lo="0x80008E0C", hi="0x80008E10")
        response = ask(0, "savestate", op="load", slot=args.slot)
        if not response.get("ok"):
            raise RuntimeError(f"load request failed: {response}")
        if args.delay_host_after_guest_apply_ms:
            wait_until(lambda: f"guest load slot={args.slot}" in log_text(1)
                       and "applying" in log_text(1), 20, "guest load apply")
            os.kill(procs[0].pid, signal.SIGSTOP)
            try:
                time.sleep(args.delay_host_after_guest_apply_ms / 1000)
            finally:
                os.kill(procs[0].pid, signal.SIGCONT)
        wait_until(lambda: all(ask(i, "savestate_status").get("last_ok")
                               for i in range(args.peers)), 30, "load apply")
        if args.fresh_link:
            def converged():
                rows = [digests(i) for i in range(args.peers)]
                common = set.intersection(*(set(row) for row in rows))
                return any(t >= 96 and len({row[t] for row in rows}) == 1
                           for t in common)
            wait_until(converged, 20, "matching post-load digest")
            if not args.resume_race:
                for menu_step, buttons in enumerate(menu_buttons):
                    start = ask(0, "frame")["frame"]
                    ask(0, "set_input", buttons=hex(buttons))
                    wait_until(lambda: ask(0, "frame")["frame"] >= start + 8,
                               10, "menu button hold")
                    ask(0, "set_input", buttons="0xFFFF")
                    wait_until(lambda: ask(0, "frame")["frame"] >= start + args.menu_gap_frames,
                               10, "menu button release")
                    if args.menu_screenshot_each:
                        ask(0, "screenshot", path=str(args.menu_screenshot.with_name(
                            f"{args.menu_screenshot.stem}.step{menu_step}"
                            f"{args.menu_screenshot.suffix}")))
                if args.menu_screenshot:
                    ask(0, "screenshot", path=str(args.menu_screenshot))
                if args.menu_only:
                    return
                ask(0, "set_input", buttons="0xDFFF")  # Circle accepts Course Select.
                start = ask(0, "frame")["frame"]
                wait_until(lambda: ask(0, "frame")["frame"] >= start + 10,
                           10, "Circle release")
                ask(0, "set_input", buttons="0xFFFF")
            try:
                wait_until(lambda: all(struct.unpack("<H", read(i, 0x800AC754, 2))[0] == args.peers
                                       and struct.unpack("<I", read(i, 0x800FF860, 4))[0] == 2
                                       for i in range(args.peers)), args.race_timeout,
                           "link race")
            except TimeoutError:
                ask(0, "screenshot", path=str(args.report.with_suffix(".race-timeout.png")))
                states = [{"frame": ask(i, "frame")["frame"],
                           "car_count": struct.unpack("<H", read(i, 0x800AC754, 2))[0],
                           "race_phase": struct.unpack("<I", read(i, 0x800FF860, 4))[0]}
                          for i in range(args.peers)]
                args.report.with_suffix(".race-timeout.json").write_text(
                    json.dumps(states, indent=2) + "\n")
                raise
            for i, buttons in enumerate(race_buttons[:args.peers]):
                ask(i, "set_input", buttons=hex(buttons))
        race_telemetry = []
        phase_change_sim = None
        if args.race_telemetry_interval:
            if not args.fresh_link:
                raise ValueError("race telemetry requires --fresh-link")
            driver_segment = args.driver_start_segment
            driver_marker = None
            driver_braking = False
            driver_still_samples = 0
            driver_recovery_left = 0
            driver_recovery_count = 0
            driver_hook_phase = 0
            driver_image = args.report.with_suffix(".driver.png")
            driver_trace = args.report.with_suffix(".telemetry.jsonl")
            if args.minimap_driver:
                driver_trace.write_text("")
            next_sample = max(digests(0), default=0) + args.race_telemetry_interval
            deadline = time.monotonic() + args.sim_timeout
            while min(max(digests(i), default=-1) for i in range(args.peers)) < args.through_sim:
                if any(proc.poll() is not None for proc in procs):
                    raise RuntimeError("peer exited during race telemetry")
                if time.monotonic() >= deadline:
                    raise TimeoutError(f"race telemetry sim {args.through_sim}")
                current = min(max(digests(i), default=-1) for i in range(args.peers))
                if phase_change_sim is not None:
                    if current >= phase_change_sim + args.phase_settle_sim:
                        break
                    time.sleep(0.05)
                    continue
                if current < next_sample:
                    time.sleep(0.05)
                    continue
                if (args.stop_on_phase_change and
                        struct.unpack("<I", read(0, 0x800FF860, 4))[0] != 2):
                    phase_change_sim = current
                    ask(0, "set_input", buttons="0xFFFF")
                    if not args.phase_settle_sim:
                        break
                    continue
                pointers = struct.unpack("<4I", read(0, 0x800FFDD0, 16))
                sample = dict(sim=current, cars=[])
                for ptr in pointers[:args.peers]:
                    car = read(0, ptr, 0x280)
                    sample["cars"].append(dict(
                        x=struct.unpack_from("<i", car, 0)[0],
                        z=struct.unpack_from("<i", car, 8)[0],
                        yaw=struct.unpack_from("<H", car, 0x54)[0],
                        speed_raw=struct.unpack_from("<h", car, 0x1D8)[0],
                        progress_candidate=struct.unpack_from("<i", car, 0x1E0)[0],
                        gear=struct.unpack_from("<h", car, 0x27A)[0]))
                if args.minimap_driver:
                    ask(0, "screenshot", path=str(driver_image))
                    marker = minimap_marker(driver_image, driver_marker,
                                            args.driver_start_segment)
                    if driver_marker is not None and math.dist(marker, driver_marker) < 0.75:
                        driver_still_samples += 1
                    else:
                        driver_still_samples = 0
                    driver_marker = marker
                    if (driver_hook_phase == 3 and marker[0] <= 42 and
                            marker[1] >= 99):
                        driver_hook_phase = 0
                        driver_segment = 0
                    driver_segment, steering, error, desired = minimap_steering(
                        marker, sample["cars"][0]["yaw"], driver_segment)
                    if (args.driver_top_straight_heading is not None and
                            driver_segment == 9 and marker[0] <= 50.5 and
                            marker[1] <= 72.5):
                        desired = args.driver_top_straight_heading % 4096
                        error = (desired - sample["cars"][0]["yaw"] + 2048) % 4096 - 2048
                        steering = (0 if abs(error) < args.driver_top_straight_band
                                    else (1 if error < 0 else 2))
                    if args.driver_hook_east_south:
                        if driver_hook_phase == 0 and driver_segment >= 14:
                            driver_hook_phase = 1
                        if driver_hook_phase == 1 and marker[0] >= 59:
                            driver_hook_phase = 2
                        if driver_hook_phase == 2 and marker[1] >= 99:
                            driver_hook_phase = 3
                        if driver_hook_phase:
                            desired = (1024, 2048, 3072)[driver_hook_phase - 1]
                    elif args.driver_hook_east and driver_segment >= 14:
                        desired = 1024
                    if (args.driver_hook_east_south and driver_hook_phase or
                            args.driver_hook_east and driver_segment >= 14):
                        error = (desired - sample["cars"][0]["yaw"] + 2048) % 4096 - 2048
                        steering = 0 if abs(error) < 96 else (1 if error < 0 else 2)
                    if driver_segment in (13, 14, 15):
                        if sample["cars"][0]["speed_raw"] > 400:
                            driver_braking = True
                        elif sample["cars"][0]["speed_raw"] < 250:
                            driver_braking = False
                    else:
                        driver_braking = False
                    buttons = ((0x7FFF, 0x7F7F, 0x7FDF) if driver_braking else
                               (0xBFFF, 0xBF7F, 0xBFDF))[steering]
                    if (args.driver_recovery and driver_recovery_left == 0 and
                            driver_still_samples >= 16 and
                            driver_still_samples % 64 == 16):
                        driver_recovery_left = 20
                        driver_recovery_count += 1
                    recovering = driver_recovery_left > 0
                    if recovering:
                        buttons = 0xBFDF if driver_recovery_count % 2 else 0xBF7F
                        driver_recovery_left -= 1
                    ask(0, "set_input", buttons=hex(buttons))
                    sample["driver"] = dict(marker=marker, segment=driver_segment,
                                            steering=steering, heading_error=error,
                                            desired_yaw=desired, braking=driver_braking,
                                            buttons=hex(buttons), recovering=recovering,
                                            still_samples=driver_still_samples,
                                            hook_phase=driver_hook_phase,
                                            recovery_count=driver_recovery_count)
                race_telemetry.append(sample)
                if args.minimap_driver:
                    with driver_trace.open("a") as trace:
                        trace.write(json.dumps(sample) + "\n")
                next_sample = current + args.race_telemetry_interval
        else:
            wait_until(lambda: all(max(digests(i), default=-1) >= args.through_sim
                                   for i in range(args.peers)), args.sim_timeout,
                       f"sim {args.through_sim}")
        if args.escape_probe:
            escape_at = time.time()
            pid = procs[visible_peer].pid
            action = subprocess.run([
                "osascript", "-e", "tell application \"System Events\" to "
                f"set frontmost of (first process whose unix id is {pid}) to true",
                "-e", "tell application \"System Events\" to key code 53",
            ], capture_output=True, text=True, timeout=8)
            end = time.monotonic() + 20
            while time.monotonic() < end and any(proc.poll() is None for proc in procs):
                time.sleep(0.05)
            reports = []
            for i in range(args.peers):
                path = Path(prefix + str(i)) / "psx_last_run_report.json"
                reports.append(json.loads(path.read_text()).get("exit_origin")
                               if path.exists() and path.stat().st_mtime >= escape_at
                               else None)
            receipt = dict(action_exit=action.returncode,
                           action_stderr=action.stderr.strip(),
                           peer_exit_codes=[proc.poll() for proc in procs],
                           exit_origins=reports,
                           elapsed_s=round(time.time() - escape_at, 3))
            args.report.write_text(json.dumps(receipt, indent=2) + "\n")
            print(json.dumps(receipt))
            if (action.returncode or any(code != 0 for code in receipt["peer_exit_codes"])
                    or reports != ["netplay_escape" if i == visible_peer
                                   else "netplay_peer_disconnect"
                                   for i in range(args.peers)]):
                raise SystemExit(1)
            return
        rows = [digests(i) for i in range(args.peers)]
        observed_sim = min(max(row, default=-1) for row in rows)
        checkpoints = range(0, min(args.through_sim, observed_sim) + 1, 32)
        mismatch = [t for t in checkpoints if any(t not in row for row in rows)
                    or len({row[t] for row in rows}) != 1]
        misses = [ask(i, "dispatch_stats")["miss_total"] for i in range(args.peers)]
        null_pc = [log_text(i).count("rb recover null-pc") for i in range(args.peers)]
        result = dict(peers=args.peers, mode=args.mode, save_slot=args.slot,
                      through_sim=args.through_sim, observed_sim=observed_sim,
                      mismatch=mismatch,
                      dispatch_misses=misses, null_pc=null_pc)
        if phase_change_sim is not None:
            result["phase_change_sim"] = phase_change_sim
        if race_telemetry:
            result["race_telemetry"] = race_telemetry
            if args.minimap_driver:
                result["driver_trace"] = str(driver_trace)
        if args.fresh_link:
            result["game_state"] = [dict(
                mode=struct.unpack("<H", read(i, 0x800F4EF4, 2))[0],
                cars=struct.unpack("<H", read(i, 0x800AC754, 2))[0],
                phase=struct.unpack("<I", read(i, 0x800FF860, 4))[0],
                accepted=list(read(i, 0x800AC074, 4)),
                active=[struct.unpack("<h", read(i, ptr + 0x1E8, 2))[0] != -1
                        for ptr in struct.unpack("<4I", read(i, 0x800FFDD0, 16))],
                positions_x=[struct.unpack("<i", read(i, ptr, 4))[0]
                             for ptr in struct.unpack("<4I", read(i, 0x800FFDD0, 16))],
                speed_raw=[struct.unpack("<h", read(i, ptr + 0x1D8, 2))[0]
                           for ptr in struct.unpack("<4I", read(i, 0x800FFDD0, 16))])
                for i in range(args.peers)]
            expected_active = [True] * args.peers + [False] * (4 - args.peers)
            expected_accepted = [1] * args.peers + [0] * (4 - args.peers)
            result["roster_ok"] = (None if phase_change_sim is not None else all(
                state["mode"] == 4 and state["cars"] == args.peers and
                state["phase"] == 2 and state["accepted"] == expected_accepted and
                state["active"] == expected_active
                for state in result["game_state"]))
        if args.gpu_report:
            ring = ask(0, "gpu_ring_stats")
            newest = ring.get("newest_frame", 0)
            frames = [ask(0, "gpu_frame_dump", frame=frame, count=8192)
                      for frame in range(max(0, newest - 7), newest + 1)]
            args.gpu_report.write_text(json.dumps(dict(ring=ring, frames=frames)))
            result["gpu_report"] = str(args.gpu_report)
            if args.fresh_link:
                result["gpu_laps"] = [top_view_lap_digits(frame) for frame in frames
                                      if frame.get("count", 0) > 0][-2:]
            if args.fresh_link and phase_change_sim is None:
                result["gpu_view_draws"] = view_draw_counts(frames, args.peers)
                recent = [frame for frame in frames if frame.get("count", 0) > 0][-2:]
                result["gpu_view_frames"] = [dict(
                    frame=frame["frame"], draws=view_draw_counts([frame], args.peers),
                    speed=view_speed_digits(frame, args.peers),
                    gear=view_gear_digits(frame, args.peers),
                    laps=top_view_lap_digits(frame))
                    for frame in recent]
                result["gpu_views_ok"] = len(recent) == 2 and all(
                    all(item["draws"].values()) for item in result["gpu_view_frames"])
                result["gpu_speed_ok"] = len(recent) == 2 and all(
                    all(value is not None for value in item["speed"])
                    for item in result["gpu_view_frames"])
                result["gpu_gear_ok"] = len(recent) == 2 and all(
                    all(value is not None for value in item["gear"])
                    for item in result["gpu_view_frames"])
        if args.screenshot:
            result["screenshot"] = ask(0, "screenshot", path=str(args.screenshot))
        if result_buttons:
            result["result_taps"] = []
            for index, buttons in enumerate(result_buttons):
                start = ask(0, "frame")["frame"]
                ask(0, "set_input", buttons=hex(buttons))
                wait_until(lambda: ask(0, "frame")["frame"] >= start + 8,
                           10, "result button hold")
                ask(0, "set_input", buttons="0xFFFF")
                wait_until(lambda: ask(0, "frame")["frame"] >= start + 108,
                           15, "result button release")
                picture = args.report.with_suffix(f".result{index}.png")
                ask(0, "screenshot", path=str(picture))
                result["result_taps"].append(dict(
                    buttons=hex(buttons), screenshot=str(picture),
                    phase=[struct.unpack("<I", read(i, 0x800FF860, 4))[0]
                           for i in range(args.peers)],
                    epc=[ask(i, "get_registers")["cop0_epc"]
                         for i in range(args.peers)]))
        if args.hud_packet_dump:
            buffer = struct.unpack("<I", read(0, 0x800ACDCC, 4))[0]
            packet_tail = read(0, buffer + 0x225D0, 0x1A8)
            speed_records = read(0, 0x8010C0B8, 4 * 20)
            args.hud_packet_dump.write_text(json.dumps(dict(
                buffer=hex(buffer), start=hex(buffer + 0x225D0),
                speed_records=[dict(index=i,
                                    words=[hex(x) for x in struct.unpack_from(
                                        "<5I", speed_records, i * 20)])
                               for i in range(4)],
                words=[dict(addr=hex(buffer + 0x225D0 + i),
                            value=hex(struct.unpack_from("<I", packet_tail, i)[0]))
                       for i in range(0, len(packet_tail), 4)])))
            result["hud_packet_dump"] = str(args.hud_packet_dump)
        if args.hud_write_trace:
            trace = ask(0, "wtrace_dump", count=2048, newest=1)
            args.hud_write_trace.write_text(json.dumps(trace))
            result["hud_write_trace"] = str(args.hud_write_trace)
            result["hud_write_coverage"] = any(
                entry["pc"] in ("0x80021A74", "0x80021AD4", "0x80021C28") and
                entry["s1"] in ("0x00000000", "0x00000001")
                for entry in trace.get("entries", []))
            result["hud_extra_seat_overflow"] = [entry for entry in
                trace.get("entries", []) if
                0xF2B90 <= int(entry["addr"], 16) < 0xF4000 and
                entry["pc"] in ("0x80021A74", "0x80021AD4", "0x80021C28") and
                entry["s1"] in ("0x00000002", "0x00000003")]
        if args.event_write_trace:
            event_trace = ask(0, "wtrace_dump", count=2048, newest=1)
            args.event_write_trace.write_text(json.dumps(event_trace, indent=2) + "\n")
            result["event_write_trace"] = str(args.event_write_trace)
            result["event_slot0_status"] = [
                struct.unpack("<I", read(i, 0x80008E0C, 4))[0]
                for i in range(args.peers)]
        if args.pause_screenshot:
            def car_positions():
                return [[struct.unpack("<2i", read(i, ptr, 8))
                         for ptr in struct.unpack("<4I", read(i, 0x800FFDD0, 16))]
                        for i in range(args.peers)]
            result["pause_before_byte"] = [read(i, 0x800F4E18, 1)[0]
                                           for i in range(args.peers)]
            before = ask(0, "frame")["frame"]
            pause_peers = range(args.peers) if args.pause_all else (args.pause_seat,)
            for i in pause_peers:
                ask(i, "set_input", buttons="0xFFF7")  # Start, active low.
            wait_until(lambda: ask(0, "frame")["frame"] >= before + 8,
                       10, "pause button hold")
            for i in pause_peers:
                ask(i, "set_input", buttons="0xFFFF")
            wait_until(lambda: ask(0, "frame")["frame"] >= before + 20,
                       10, "pause display")
            result["pause_screenshot"] = ask(
                0, "screenshot", path=str(args.pause_screenshot))
            result["pause_state"] = [dict(
                mode=struct.unpack("<H", read(i, 0x800F4EF4, 2))[0],
                phase=struct.unpack("<I", read(i, 0x800FF860, 4))[0],
                pause_byte=read(i, 0x800F4E18, 1)[0])
                for i in range(args.peers)]
            paused_positions = car_positions()
            before = ask(0, "frame")["frame"]
            wait_until(lambda: ask(0, "frame")["frame"] >= before + 64,
                       10, "paused simulation hold")
            result["paused_cars_still"] = car_positions() == paused_positions
            result["pause_held_bytes"] = [read(i, 0x800F4E18, 1)[0]
                                           for i in range(args.peers)]
            if args.pause_menu_probe:
                def menu_indexes():
                    return [read(i, 0x800F318C, 1)[0]
                            for i in range(args.peers)]
                def menu_direction(seat, buttons, expected, label):
                    begin = ask(0, "frame")["frame"]
                    ask(seat, "set_input", buttons=hex(buttons))
                    wait_until(lambda: ask(0, "frame")["frame"] >= begin + 8,
                               10, f"{label} hold")
                    ask(seat, "set_input", buttons="0xFFFF")
                    wait_until(lambda: menu_indexes() == [expected] * args.peers,
                               10, f"synchronized pause {label}")
                    return dict(seat=seat, indexes=menu_indexes(),
                                screenshot=ask(0, "screenshot", path=str(
                                    args.pause_screenshot.with_name(
                                        f"{args.pause_screenshot.stem}-{label}"
                                        f"{args.pause_screenshot.suffix}"))))
                down_seat = (args.pause_seat + 1) % args.peers
                up_seat = (args.pause_seat + 2) % args.peers
                result["pause_menu"] = dict(
                    initial=menu_indexes(),
                    down=menu_direction(down_seat, 0xFFBF, 1, "down"),
                    up=menu_direction(up_seat, 0xFFEF, 0, "up"))
                result["pause_menu"]["cars_still"] = (
                    car_positions() == paused_positions)
            if args.resume_screenshot:
                before = ask(0, "frame")["frame"]
                for i in pause_peers:
                    ask(i, "set_input", buttons="0xFFF7")
                wait_until(lambda: ask(0, "frame")["frame"] >= before + 8,
                           10, "resume button hold")
                for i in pause_peers:
                    ask(i, "set_input", buttons="0xFFFF")
                wait_until(lambda: ask(0, "frame")["frame"] >= before + 20,
                           10, "resume display")
                result["resume_screenshot"] = ask(
                    0, "screenshot", path=str(args.resume_screenshot))
                result["resume_state"] = [dict(
                    mode=struct.unpack("<H", read(i, 0x800F4EF4, 2))[0],
                    phase=struct.unpack("<I", read(i, 0x800FF860, 4))[0],
                    pause_byte=read(i, 0x800F4E18, 1)[0])
                    for i in range(args.peers)]
                result["resumed_cars_moved"] = car_positions() != paused_positions
            rows = [digests(i) for i in range(args.peers)]
            common = set.intersection(*(set(row) for row in rows))
            result["post_pause_mismatch"] = [tick for tick in sorted(common)
                                              if len({row[tick] for row in rows}) != 1]
        if args.save_checkpoint_slot is not None:
            generations = [ask(i, "savestate_status")["generation"]
                           for i in range(args.peers)]
            response = ask(0, "savestate", op="save", slot=args.save_checkpoint_slot)
            if not response.get("ok"):
                raise RuntimeError(f"checkpoint save request failed: {response}")
            wait_until(lambda: all(
                ask(i, "savestate_status")["generation"] > generations[i] and
                ask(i, "savestate_status")["last_ok"]
                for i in range(args.peers)), 90, "synchronized checkpoint save")
            result["checkpoint_save"] = dict(slot=args.save_checkpoint_slot,
                                              generations=[ask(i, "savestate_status")
                                                           ["generation"]
                                                           for i in range(args.peers)])
        result["repeated_loads"] = []
        for attempt in range(2, args.repeat_loads + 1):
            generations = [ask(i, "savestate_status")["generation"]
                           for i in range(args.peers)]
            offsets = [len(log_text(i)) for i in range(args.peers)]
            if args.delay_guest_ms:
                os.kill(procs[1].pid, signal.SIGSTOP)
            try:
                response = ask(0, "savestate", op="load", slot=args.slot)
                if not response.get("ok"):
                    raise RuntimeError(f"repeat load request failed: {response}")
                if args.delay_guest_ms:
                    time.sleep(args.delay_guest_ms / 1000)
            finally:
                if args.delay_guest_ms:
                    os.kill(procs[1].pid, signal.SIGCONT)

            def resumed_rows(i):
                suffix = log_text(i)[offsets[i]:]
                marker = suffix.rfind("peer ready, resuming lockstep")
                if marker < 0:
                    return {}
                suffix = suffix[marker:]
                return {int(tick): core for tick, core in re.findall(
                    r"rb live dig local sim=(\d+) core=([0-9a-f]+)", suffix)}

            wait_until(lambda: all(
                ask(i, "savestate_status")["generation"] > generations[i]
                and ask(i, "savestate_status")["last_ok"]
                and max(resumed_rows(i), default=-1) >= args.through_sim
                for i in range(args.peers)), 50,
                f"repeated load {attempt} through sim {args.through_sim}")
            repeated = [resumed_rows(i) for i in range(args.peers)]
            repeated_mismatch = [t for t in checkpoints
                                 if any(t not in row for row in repeated)
                                 or len({row[t] for row in repeated}) != 1]
            item = dict(load=attempt, mismatch=repeated_mismatch,
                        dispatch_misses=[ask(i, "dispatch_stats")["miss_total"]
                                         for i in range(args.peers)],
                        null_pc=[log_text(i)[offsets[i]:].count("rb recover null-pc")
                                 for i in range(args.peers)])
            result["repeated_loads"].append(item)
            if repeated_mismatch or any(item["dispatch_misses"]) or any(item["null_pc"]):
                raise RuntimeError(f"repeated load failed: {item}")
        args.report.write_text(json.dumps(result, indent=2) + "\n")
        print(json.dumps(result))
        if (mismatch or any(misses) or any(null_pc) or
                (args.pause_screenshot and
                 (not all(row["pause_byte"] == 1 for row in result["pause_state"])
                  or not result["paused_cars_still"]
                  or not all(value == 1 for value in result["pause_held_bytes"])
                  or result["post_pause_mismatch"]
                  or (args.pause_menu_probe and
                      (result["pause_menu"]["initial"] != [0] * args.peers or
                       not result["pause_menu"]["cars_still"]))
                  or (args.resume_screenshot and
                      (not all(row["pause_byte"] == 0 for row in result["resume_state"])
                       or not result["resumed_cars_moved"])))) or
                result.get("roster_ok") is False or result.get("gpu_views_ok") is False or
                result.get("gpu_speed_ok") is False or
                result.get("gpu_gear_ok") is False or
                result.get("hud_write_coverage") is False or
                result.get("hud_extra_seat_overflow")):
            raise SystemExit(1)
    finally:
        for proc in procs:
            if proc.poll() is None:
                os.kill(proc.pid, signal.SIGCONT)
                proc.terminate()
        for proc in procs:
            try:
                proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
        for handle in handles:
            handle.close()


if __name__ == "__main__":
    main()

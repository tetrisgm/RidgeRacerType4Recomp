#!/usr/bin/env python3
"""Compare R4 guest steering response from one saved race state.

Run a debug build with ``--headless`` and use ``--start-grid --pad-type N``.
It keeps one controller identity active from boot, saves at the stationary grid,
then replays the exact same state for neutral/partial/full steering with
throttle. It records R4-owned position, yaw and speed as well as SIO identity
and calibrated JogCon steering. Screenshots are supplementary. ``--boot-to-race``
still runs the original moving-race route, which uses a looser coast baseline.
"""
import json
import os
import sys
import time

from dbg import DEFAULT_PORT, cmd, send
from pad import mask
from smoke import START_FRAME, STEPS

CAR0 = 0x800AC0B0
CAR_READ_LEN = 0x1DA  # through the signed speed field at +0x1D8
CONTROLS = (("neutral", 128), ("partial-left", 64), ("full-left", 0),
            ("partial-right", 192), ("full-right", 255))


def wait_frames(count, port):
    target = cmd({"cmd": "frame"}, port=port).get("frame", 0) + count
    deadline = time.time() + max(15, count / 30)
    while time.time() < deadline:
        if cmd({"cmd": "frame"}, port=port).get("frame", 0) >= target:
            return
        time.sleep(0.01)
    raise RuntimeError(f"runtime did not advance {count} frames")


def wait_savestate(op, slot, port):
    before = cmd({"cmd": "savestate_status"}, port=port)
    generation = before.get("generation", 0)
    request = cmd({"cmd": "savestate", "op": op, "slot": slot}, port=port)
    if not request.get("ok"):
        raise RuntimeError(f"savestate {op} failed: {request}")
    deadline = time.time() + 15
    while time.time() < deadline:
        state = cmd({"cmd": "savestate_status"}, port=port)
        if state.get("generation", 0) > generation and state.get("pending", 0) == 0:
            if state.get("last_ok") != 1 or state.get("last_op") != op:
                raise RuntimeError(f"savestate {op} did not complete: {state}")
            return state
        time.sleep(0.01)
    raise RuntimeError(f"timed out waiting for savestate {op}")


def read_car_state(port):
    response = cmd({"cmd": "read_ram", "addr": hex(CAR0), "len": CAR_READ_LEN}, port=port)
    if not response.get("ok"):
        raise RuntimeError(f"car-state read failed: {response}")
    raw = bytes.fromhex(response["hex"])

    def u16(offset):
        return int.from_bytes(raw[offset:offset + 2], "little")

    def s16(offset):
        return int.from_bytes(raw[offset:offset + 2], "little", signed=True)

    def s32(offset):
        return int.from_bytes(raw[offset:offset + 4], "little", signed=True)

    return {"world_x": s32(0x10), "world_z": s32(0x18),
            "yaw_12bit": u16(0x54) & 0x0FFF,
            "speed_raw": s16(0x1D8)}


def read_jogcon_state(port):
    """Read R4's calibrated steering and signed wheel accumulator fields."""
    response = cmd({"cmd": "read_ram", "addr": "0x800F3BE8", "len": 0x50}, port=port)
    if not response.get("ok"):
        raise RuntimeError(f"JogCon state read failed: {response}")
    raw = bytes.fromhex(response["hex"])
    return {
        "guest_pad_id": raw[1],
        "mode": int.from_bytes(raw[0x10:0x14], "little"),
        "normalized_steering": int.from_bytes(raw[0x3C:0x3E], "little", signed=True),
        "wheel_word_4c": int.from_bytes(raw[0x4C:0x50], "little", signed=True),
    }


def boot_to_race(port, type_id, trace_path, stop_at_grid=False):
    """Use the smoke.py menu sequence while keeping this pad type from boot."""
    released = {"buttons": "0xFFFF", "pad_type": type_id,
                "lx": 128, "ly": 128, "rx": 128, "ry": 128}
    cmd({"cmd": "set_input", **released}, port=port)
    # Preserve the controller's first boot-time identification exchanges,
    # before the menu-driving sequence can wrap the finite SIO trace ring.
    wait_frames(2, port)
    trace = send(json.dumps({"cmd": "sio_trace", "count": 128}), port=port)
    with open(trace_path, "w", encoding="utf-8") as f:
        f.write(trace)
        f.write("\n")
    cmd({"cmd": "turbo", "enabled": 1}, port=port)
    frame = cmd({"cmd": "frame"}, port=port).get("frame", 0)
    initial_frames = max(0, START_FRAME - frame)
    wait_frames(min(120, initial_frames), port)
    # Capture the device's first ID/config handshake before the long scripted
    # menu route cycles enough polls to roll the trace ring over.
    write_sio_trace(os.path.join(os.path.dirname(trace_path),
                                 "boot-handshake-sio-trace.json"),
                    port, count=1024)
    frame = cmd({"cmd": "frame"}, port=port).get("frame", 0)
    wait_frames(max(0, START_FRAME - frame), port)
    # A moving race car is a poor baseline: even correct savestate restores
    # can differ by a few speed units after a neutral coast. The cold-start
    # acceptance path saves at the stationary grid before the first throttle
    # input, so every replay can require an exact guest-RAM baseline.
    steps = STEPS[:-4] if stop_at_grid else STEPS
    for label, buttons, hold, settle in steps:
        if buttons:
            pressed = dict(released)
            pressed["buttons"] = "0x%04X" % mask(buttons)
            cmd({"cmd": "set_input", **pressed}, port=port)
            wait_frames(int(hold * 60), port)
            cmd({"cmd": "set_input", **released}, port=port)
        wait_frames(int(settle * 60), port)
    cmd({"cmd": "turbo", "enabled": 0}, port=port)
    status = cmd({"cmd": "pad_status"}, port=port).get("slot0", {})
    if status.get("type") != type_id:
        raise RuntimeError(f"controller type was not held during boot: {status}")


def signed_yaw_delta(before, after):
    return ((after - before + 0x800) & 0xFFF) - 0x800


def write_results(out_dir, checks):
    with open(os.path.join(out_dir, "samples.json"), "w", encoding="utf-8") as f:
        json.dump(checks, f, indent=2)
        f.write("\n")


def write_sio_trace(path, port, count=512):
    raw = send(json.dumps({"cmd": "sio_trace", "count": count}), port=port)
    with open(path, "w", encoding="utf-8") as f:
        f.write(raw)
        f.write("\n")


def main():
    out_dir = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "/tmp/jogcon-r4-capture")
    port = int(sys.argv[sys.argv.index("--port") + 1]) if "--port" in sys.argv else DEFAULT_PORT
    os.makedirs(out_dir, exist_ok=True)
    deadline = time.time() + 30
    while time.time() < deadline:
        try:
            if cmd({"cmd": "ping"}, port=port, timeout=1.0).get("ok"):
                break
        except OSError:
            pass
        time.sleep(0.05)
    else:
        raise RuntimeError(f"debug server unavailable on port {port}")
    selected_type = (int(sys.argv[sys.argv.index("--pad-type") + 1])
                     if "--pad-type" in sys.argv else None)
    if selected_type is not None and selected_type not in (1, 2):
        raise RuntimeError("--pad-type must be 1 (DualShock) or 2 (JogCon)")
    if selected_type is None:
        raise RuntimeError("--pad-type is required; test one device identity per runtime boot")
    if "--boot-to-race" in sys.argv or "--start-grid" in sys.argv:
        if selected_type is None:
            raise RuntimeError("boot routing requires --pad-type so the device is present from boot")
        boot_to_race(port, selected_type,
                     os.path.join(out_dir, "boot-sio-trace.json"),
                     stop_at_grid="--start-grid" in sys.argv)
        write_sio_trace(os.path.join(out_dir, "race-entry-sio-trace.json"), port)
    cmd({"cmd": "turbo", "enabled": 0}, port=port)

    # Reuse one race snapshot for each sample so the input comparison starts
    # from the same vehicle position and speed. Save after the caller has
    # reached a playable race scene; the load ack is sent before it completes,
    # so wait for the completion generation from savestate_status.
    slot_num = int(sys.argv[sys.argv.index("--slot") + 1]) if "--slot" in sys.argv else 9
    cmd({"cmd": "set_input", "buttons": "0xFFFF", "lx": 128,
         "ly": 128, "rx": 128, "ry": 128,
         "pad_type": selected_type}, port=port)
    if "--reuse-slot" not in sys.argv:
        wait_savestate("save", slot_num, port)

    checks = []
    reference_baseline = None
    modes = ((selected_type, "dualshock" if selected_type == 1 else "jogcon"),)
    for type_id, name in modes:
        for label, x in CONTROLS:
            cmd({"cmd": "set_input", "buttons": "0xFFFF", "lx": 128,
                 "ly": 128, "rx": 128, "ry": 128,
                 "pad_type": type_id}, port=port)
            wait_savestate("load", slot_num, port)
            # Restore guard is 90..700 ms. This fixed neutral lead-in clears
            # it and gives every trial the same number of guest race frames.
            wait_frames(110, port)
            baseline = read_car_state(port)
            if reference_baseline is None:
                reference_baseline = baseline
            elif any(abs(baseline[key] - reference_baseline[key]) > limit
                     for key, limit in (("world_x", 0 if "--start-grid" in sys.argv else 128),
                                        ("world_z", 0 if "--start-grid" in sys.argv else 128),
                                        ("yaw_12bit", 0 if "--start-grid" in sys.argv else 3),
                                        ("speed_raw", 0 if "--start-grid" in sys.argv else 8))):
                raise RuntimeError(
                    f"race replay baseline drifted for {name}/{label}: "
                    f"expected near {reference_baseline}, got {baseline}")
            accepted = cmd({"cmd": "set_input", "buttons": "0xBFFF",
                            "lx": x, "ly": 128, "rx": 128, "ry": 128,
                            "pad_type": type_id}, port=port)
            if not accepted.get("ok"):
                raise RuntimeError(f"set_input rejected {name}/{label}: {accepted}")
            after5 = None
            if type_id == 2:
                wait_frames(5, port)
                after5 = read_jogcon_state(port)
                wait_frames(55, port)
            else:
                wait_frames(60, port)
            middle = read_car_state(port)
            wait_frames(60, port)
            end = read_car_state(port)
            wait_frames(120, port)
            end240 = read_car_state(port)
            write_sio_trace(
                os.path.join(out_dir, f"{name}-{label}-sio-trace.json"),
                port, count=32)
            status = cmd({"cmd": "pad_status"}, port=port)
            slot_state = status.get("slot0", {})
            if slot_state.get("type") != type_id or slot_state.get("sticks", [None])[0] != x:
                raise RuntimeError(f"unexpected {name}/{label} state: {status}")
            frame = cmd({"cmd": "frame"}, port=port).get("frame")
            image_path = None
            if "--screenshots" in sys.argv:
                image_path = os.path.join(out_dir, f"{name}-{label}-f{frame}.png")
                shot = cmd({"cmd": "screenshot", "path": image_path}, port=port)
                if not shot.get("ok"):
                    raise RuntimeError(f"screenshot failed: {shot}")
            checks.append({"name": name, "control": label, "frame": frame,
                           "axis_x": x, "pad_type": type_id,
                           "pad_status": status,
                           "sio_state": cmd({"cmd": "sio_state"}, port=port),
                           "baseline": baseline, "after5": after5, "after60": middle,
                           "after120": end, "after240": end240,
                           "delta120": {
                               "world_x": end["world_x"] - baseline["world_x"],
                               "world_z": end["world_z"] - baseline["world_z"],
                               "yaw": signed_yaw_delta(baseline["yaw_12bit"], end["yaw_12bit"]),
                               "speed_raw": end["speed_raw"] - baseline["speed_raw"],
                           }, "image": image_path})
            write_results(out_dir, checks)

    # If R4 ignored the steering value, opposite full-axis trials from an
    # identical initial state and identical throttle duration would produce
    # identical guest car state. Compare game-owned RAM, not the injected pad.
    for type_id, name in modes:
        trials = {item["control"]: item for item in checks if item["name"] == name}
        left = trials["full-left"]["after240"]
        right = trials["full-right"]["after240"]
        if left == right:
            raise RuntimeError(f"R4 guest car state did not respond to {name} steering")
        neutral = trials["neutral"]
        if neutral["after120"]["speed_raw"] <= neutral["baseline"]["speed_raw"]:
            raise RuntimeError(f"R4 {name} Cross input did not accelerate from the start grid")
        left_delta = trials["full-left"]["delta120"]["yaw"]
        right_delta = trials["full-right"]["delta120"]["yaw"]
        if left_delta == 0 or right_delta == 0 or (left_delta > 0) == (right_delta > 0):
            raise RuntimeError(
                f"R4 {name} left/right steering did not produce opposite yaw: "
                f"left={left_delta}, right={right_delta}")

    if "--axis-sweep" in sys.argv:
        if selected_type != 2 or "--start-grid" not in sys.argv:
            raise RuntimeError("--axis-sweep requires JogCon and --start-grid")
        axis_checks = []
        reference_grid = None
        for x in (96, 112, 128, 144, 160):
            cmd({"cmd": "set_input", "buttons": "0xFFFF", "lx": 128,
                 "ly": 128, "rx": 128, "ry": 128,
                 "pad_type": selected_type}, port=port)
            wait_savestate("load", slot_num, port)
            wait_frames(110, port)
            grid = read_car_state(port)
            if reference_grid is None:
                reference_grid = grid
            elif grid != reference_grid:
                raise RuntimeError(
                    f"JogCon axis sweep restored a different grid: "
                    f"expected {reference_grid}, got {grid}")
            cmd({"cmd": "set_input", "buttons": "0xBFFF", "lx": x,
                 "ly": 128, "rx": 128, "ry": 128,
                 "pad_type": selected_type}, port=port)
            wait_frames(5, port)
            state = read_jogcon_state(port)
            axis_checks.append({"axis_x": x, "car_baseline": grid, **state})
        normalized = [item["normalized_steering"] for item in axis_checks]
        wheel_words = [item["wheel_word_4c"] for item in axis_checks]
        if not all(a < b for a, b in zip(normalized, normalized[1:])):
            raise RuntimeError(f"JogCon normalized steering is not proportional near center: {normalized}")
        if not all(a < b for a, b in zip(wheel_words, wheel_words[1:])):
            raise RuntimeError(f"JogCon wheel input is not proportional near center: {wheel_words}")
        with open(os.path.join(out_dir, "axis-sweep.json"), "w", encoding="utf-8") as f:
            json.dump(axis_checks, f, indent=2)
            f.write("\n")

    cmd({"cmd": "clear_input"}, port=port)
    print(json.dumps(checks, indent=2))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"test_jogcon_runtime: {exc}", file=sys.stderr)
        try:
            cmd({"cmd": "clear_input"})
        except Exception:
            pass
        raise SystemExit(1)

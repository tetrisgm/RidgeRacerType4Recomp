#!/usr/bin/env python3
"""Check R4 Controls (r4.modern-controls) in a live headless run.

Drives a debug build (-DPSX_DEBUG_TOOLS=ON) started with --headless through the
debug server's host-pad layer (set_input {"layer":"host"}): a virtual P1
gamepad with both triggers that feeds the normal offline input path, the title
pad transform and host shortcuts, as a physical pad would. All checks restore
one Grand Prix grid savestate, read R4-owned RAM and print one JSON report;
it is not a physical-controller test. Settings the run needs, in
<build>/settings.toml: [video] rewind = true (for the Rewind check) and
[controller] p1_mode = "analog" (stick steering).

  grid  [--slot 9] [--manual]   fresh boot -> Grand Prix grid, save the slot
                                (--manual picks the manual transmission)
  modern [--slot 9] [--mt-slot 7]
                                Modern acceptance: guest pad type, throttle,
                                brake, steering sweep, camera, Rewind, shifting
                                (shifting needs a --manual grid slot)
  classic [--slot 9]            Classic / feature off: stock pad in races,
                                Y alone is Triangle, Select + Y opens Rewind
  splitscreen --vs-slot 9 --grid-slot 7 [--classic]
                                no Rewind in 2P VS Battle (r4.split-screen-rewind):
                                in a VS race Rewind is blocked, captures nothing
                                and Y / Select + Y do not open it; back on a 1P
                                grid it captures and Y (--classic: Select + Y,
                                for Classic or Controls off) opens it. Needs a
                                2P VS race slot and a 1P grid slot
  sio OUT.json [--slot 9]       replay a fixed host script from the slot and
                                record every P1 SIO poll (Classic / mods-off A/B)
  compare A.json B.json         identical poll sequences?

Common: --port N (debug port, default 4797). Start the runtime first, e.g.
  tools/run_r4.sh build --headless --debug-port 4797
"""
import json
import os
import sys
import time

from dbg import DEFAULT_PORT, cmd, send
from pad import mask
from smoke import START_FRAME, STEPS

CAR0 = 0x800AC0B0          # player 1 car
CAR_SPEED = 0x1D8          # s16
CAR_GEAR = 0x228           # u8, 1.. (manual and automatic)
CAR_VIEW = 0x640           # u16, camera view index
PAD0 = 0x800F3BE8          # R4's decoded pad 0 (0x5C per port)
RACE_PHASE = 0x800FF860
PAUSED = 0x800F4F6C
SIO_PAD_NEGCON = 3
NEGCON_ID = 0x23

PSX = {"select": 0x0001, "start": 0x0008, "up": 0x0010, "down": 0x0040,
       "r1": 0x0800, "triangle": 0x1000, "circle": 0x2000, "square": 0x8000}

PORT = DEFAULT_PORT


def c(obj, **kw):
    return cmd(obj, port=PORT, **kw)


def frame():
    return c({"cmd": "frame"}).get("frame", 0)


def wait_frames(n):
    target = frame() + n
    deadline = time.monotonic() + max(30, n / 20)
    while frame() < target:
        if time.monotonic() > deadline:
            raise RuntimeError(f"runtime did not advance {n} frames")
        time.sleep(0.004)


def wait_up(timeout=60):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            if c({"cmd": "ping"}, timeout=1.0).get("ok"):
                return
        except OSError:
            pass
        time.sleep(0.2)
    raise RuntimeError(f"debug server unavailable on port {PORT}")


def savestate(op, slot):
    generation = c({"cmd": "savestate_status"}).get("generation", 0)
    r = c({"cmd": "savestate", "op": op, "slot": slot})
    if not r.get("ok"):
        raise RuntimeError(f"savestate {op} failed: {r}")
    deadline = time.monotonic() + 20
    while time.monotonic() < deadline:
        s = c({"cmd": "savestate_status"})
        if s.get("generation", 0) > generation and s.get("pending", 0) == 0:
            if s.get("last_ok") != 1:
                raise RuntimeError(f"savestate {op} did not complete: {s}")
            return
        time.sleep(0.01)
    raise RuntimeError(f"timed out waiting for savestate {op}")


def ram(addr, n):
    r = c({"cmd": "read_ram", "addr": hex(addr), "len": n})
    if not r.get("ok"):
        raise RuntimeError(f"read_ram failed: {r}")
    return bytes.fromhex(r["hex"])


def u32(addr):
    return int.from_bytes(ram(addr, 4), "little")


def car():
    r = ram(CAR0, CAR_VIEW + 2)
    return {"x": int.from_bytes(r[0x10:0x14], "little", signed=True),
            "z": int.from_bytes(r[0x18:0x1C], "little", signed=True),
            "yaw": int.from_bytes(r[0x54:0x56], "little") & 0xFFF,
            "speed": int.from_bytes(r[CAR_SPEED:CAR_SPEED + 2], "little", signed=True),
            "gear": r[CAR_GEAR],
            "view": int.from_bytes(r[CAR_VIEW:CAR_VIEW + 2], "little")}


def guest_pad():
    r = ram(PAD0, 0x40)
    s16 = lambda o: int.from_bytes(r[o:o + 2], "little", signed=True)
    return {"id": r[1], "mode": int.from_bytes(r[0x10:0x14], "little", signed=True),
            "buttons": int.from_bytes(r[2:4], "little"), "twist": s16(32),
            "i": s16(34), "ii": s16(36), "steering": s16(0x3C)}


def host(buttons=(), lx=128, lt=0, rt=0):
    word = 0xFFFF
    for b in buttons:
        word &= ~PSX[b]
    r = c({"cmd": "set_input", "layer": "host", "buttons": "0x%04X" % word,
           "lx": lx, "ly": 128, "rx": 128, "ry": 128, "lt": lt, "rt": rt})
    if not r.get("ok"):
        raise RuntimeError(f"host set_input rejected: {r}")


def tap(buttons, lt=0, rt=0, hold=4, settle=20):
    host(buttons, lt=lt, rt=rt)
    wait_frames(hold)
    host(lt=lt, rt=rt)
    wait_frames(settle)


def misses():
    d = c({"cmd": "dispatch_stats"})
    return {"dispatch": d.get("miss_total"), "segment": d.get("segment_miss_total")}


def restore(slot):
    """Grid state, neutral host pad; the 40-frame lead-in clears the
    savestate input guard and lets R4 re-identify the pad."""
    host()
    savestate("load", slot)
    wait_frames(40)


def yaw_delta(a, b):
    return ((b - a + 0x800) & 0xFFF) - 0x800


def check(report, name, ok, **data):
    report["checks"].append({"name": name, "ok": bool(ok), **data})
    print(("PASS " if ok else "FAIL ") + name + " " + json.dumps(data), flush=True)


# ---- grid -----------------------------------------------------------------

def cmd_grid(args):
    slot = int(opt(args, "--slot", 9))
    manual = "--manual" in args
    wait_up()
    c({"cmd": "turbo", "enabled": 1})
    wait_frames(max(0, START_FRAME - frame()))
    for label, buttons, hold, settle in STEPS[:-4]:
        if buttons:
            c({"cmd": "set_input", "buttons": "0x%04X" % mask(buttons)})
            wait_frames(int(hold * 60))
            c({"cmd": "clear_input"})
        wait_frames(int(settle * 60))
        if manual and label == "gp-11":   # AT / MT choice is up: pick MT
            c({"cmd": "set_input", "buttons": "0x%04X" % mask(["down"])})
            wait_frames(9)
            c({"cmd": "clear_input"})
            wait_frames(30)
    c({"cmd": "turbo", "enabled": 0})
    phase = u32(RACE_PHASE)
    if phase >= 4:
        raise RuntimeError(f"not on a race grid (phase {phase})")
    savestate("save", slot)
    print(json.dumps({"slot": slot, "manual": manual, "frame": frame(),
                      "phase": phase, "car": car(), "misses": misses()}))
    return 0


# ---- modern ---------------------------------------------------------------

def cmd_modern(args):
    slot = int(opt(args, "--slot", 9))
    mt_slot = opt(args, "--mt-slot", None)
    report = {"checks": []}
    wait_up()
    c({"cmd": "turbo", "enabled": 0})

    # Guest identity while driving: NeGcon, R4's NeGcon input mode, all race.
    restore(slot)
    seen = set()
    for _ in range(30):
        p = guest_pad()
        seen.add((p["id"], p["mode"], u32(PAUSED)))
        wait_frames(3)
    status = c({"cmd": "pad_status"})["slot0"]
    check(report, "race presents NeGcon (R4 pad ID 0x23, mode 1, unpaused)",
          seen == {(NEGCON_ID, 1, 0)} and status["type"] == SIO_PAD_NEGCON,
          seen=sorted(seen), sio_type=status["type"])

    # Throttle: RT 0 / 128 / 255 from the same grid, 120 frames each.
    thr = {}
    for v in (0, 128, 255):
        restore(slot)
        host(rt=v)
        wait_frames(120)
        thr[v] = {"speed": car()["speed"], "i": guest_pad()["i"]}
    check(report, "RT raises speed proportionally (0 < RT128 < RT255)",
          thr[0]["speed"] == 0 < thr[128]["speed"] < thr[255]["speed"]
          and thr[0]["i"] == 0 < thr[128]["i"] < thr[255]["i"] == 106,
          trials=thr)

    # Brake: full throttle for 150 frames, then 60 frames of LT.
    brk = {}
    for v in (0, 128, 255):
        restore(slot)
        host(rt=255)
        wait_frames(150)
        before = car()["speed"]
        host(lt=v)
        wait_frames(60)
        brk[v] = {"before": before, "after": car()["speed"], "ii": guest_pad()["ii"]}
    check(report, "LT reduces speed proportionally (coast > LT128 > LT255)",
          brk[0]["after"] > brk[128]["after"] > brk[255]["after"],
          trials=brk)

    # Steering: left stick sweep at full throttle.
    steer = {}
    sweep = (0, 32, 64, 96, 128, 160, 192, 224, 255)
    for lx in sweep:
        restore(slot)
        host(rt=255)
        wait_frames(40)
        a = car()
        host(lx=lx, rt=255)
        wait_frames(60)
        b = car()
        steer[lx] = {"steering": guest_pad()["steering"],
                     "yaw": yaw_delta(a["yaw"], b["yaw"])}
    s = [steer[k]["steering"] for k in sweep]
    y = [steer[k]["yaw"] for k in sweep]
    left, right = y[:4], y[5:]
    check(report, "left stick steers proportionally, neutral straight",
          steer[128]["steering"] == 0 and steer[128]["yaw"] == 0
          and all(p < q for p, q in zip(s, s[1:]))
          and all(abs(p) > abs(q) > 0 for p, q in zip(left, left[1:]))
          and all(0 < abs(p) < abs(q) for p, q in zip(right, right[1:]))
          and (y[0] > 0) != (y[-1] > 0),
          sweep=steer)

    # Camera: R1 toggles R4's view; host Triangle/Y never reaches the game.
    restore(slot)
    host(rt=255)
    wait_frames(30)
    views = [car()["view"]]
    for _ in range(2):
        tap(["r1"], rt=255)
        views.append(car()["view"])
    check(report, "R1 changes the camera view",
          views[0] != views[1] and views[2] == views[0], views=views)

    # Rewind: Y opens it (guest frozen), Circle cancels (guest resumes).
    restore(slot)
    host(rt=255)
    wait_frames(240)
    f0 = frame()
    host(["triangle"], rt=255)
    time.sleep(0.5)
    f1 = frame()
    time.sleep(0.5)
    f2 = frame()
    triangle_reached = (guest_pad()["buttons"] & 0x10) != 0
    host()
    time.sleep(0.2)
    host(["circle"])
    time.sleep(0.2)
    host()
    time.sleep(0.5)
    f3 = frame()
    time.sleep(0.5)
    f4 = frame()
    check(report, "Y opens Rewind (guest frozen), Circle cancels (guest resumes)",
          f1 == f2 and f1 - f0 < 10 and f4 > f3 > f2 and not triangle_reached,
          frames=[f0, f1, f2, f3, f4], triangle_reached_guest=triangle_reached)

    # Shifting (manual transmission grid): Circle up, Square down.
    if mt_slot is not None:
        restore(int(mt_slot))
        host(rt=255)
        wait_frames(30)
        gears = [car()["gear"]]
        for b in ("circle", "circle", "square"):
            tap([b], rt=255)
            gears.append(car()["gear"])
        check(report, "Circle shifts up, Square shifts down (manual)",
              gears[1] == gears[0] + 1 and gears[2] == gears[0] + 2
              and gears[3] == gears[0] + 1, gears=gears)

    # Pause menu: Start pauses; while paused the pad is the stock one.
    restore(slot)
    host(rt=255)
    wait_frames(60)
    tap(["start"])
    wait_frames(10)
    paused = u32(PAUSED)
    p = guest_pad()
    tap(["start"])
    wait_frames(10)
    check(report, "pause menu gets the stock pad",
          paused == 1 and p["id"] != NEGCON_ID and u32(PAUSED) == 0,
          paused=paused, guest_id=p["id"])

    host()
    c({"cmd": "clear_input"})
    m = misses()
    check(report, "0 dispatch and 0 segment misses",
          m["dispatch"] == 0 and m["segment"] == 0, **m)
    print(json.dumps(report, indent=1))
    return 0 if all(x["ok"] for x in report["checks"]) else 1


# ---- classic --------------------------------------------------------------

def cmd_classic(args):
    """Classic scheme or feature off: stock pad in races, Y is Select + Y."""
    slot = int(opt(args, "--slot", 9))
    report = {"checks": []}
    wait_up()
    c({"cmd": "turbo", "enabled": 0})
    restore(slot)
    host(rt=255, lt=255)
    seen = set()
    for _ in range(20):
        seen.add(guest_pad()["id"])
        wait_frames(3)
    check(report, "race keeps the stock pad (no NeGcon)",
          NEGCON_ID not in seen and c({"cmd": "pad_status"})["slot0"]["type"]
          != SIO_PAD_NEGCON, guest_ids=sorted(seen))
    host(["triangle"])
    wait_frames(6)
    reached = (guest_pad()["buttons"] & 0x10) != 0
    f0 = frame()
    wait_frames(20)
    host()
    check(report, "Y alone reaches the game as Triangle, Rewind stays shut",
          reached and frame() > f0, triangle_reached_guest=reached)
    wait_frames(240)
    host(["select", "triangle"])
    time.sleep(0.5)
    f1 = frame()
    time.sleep(0.5)
    f2 = frame()
    host()
    time.sleep(0.2)
    host(["circle"])
    time.sleep(0.2)
    host()
    time.sleep(0.5)
    f3 = frame()
    time.sleep(0.5)
    check(report, "Select + Y opens Rewind, Circle cancels",
          f1 == f2 and frame() > f3, frames=[f1, f2, f3, frame()])
    c({"cmd": "clear_input"})
    m = misses()
    check(report, "0 dispatch and 0 segment misses",
          m["dispatch"] == 0 and m["segment"] == 0, **m)
    return 0 if all(x["ok"] for x in report["checks"]) else 1


# ---- splitscreen ----------------------------------------------------------

def rewind_status():
    r = c({"cmd": "rewind_status"})
    if not r.get("ok"):
        raise RuntimeError(f"rewind_status failed (needs psxrecomp with it): {r}")
    return r


def press_and_watch(buttons):
    """Hold `buttons` for half a second; (frames advanced, Rewind open)."""
    host(buttons, rt=255)
    time.sleep(0.25)
    f1 = frame()
    time.sleep(0.5)
    f2 = frame()
    opened = rewind_status()["open"]
    host(rt=255)
    return f2 - f1, opened


def cmd_splitscreen(args):
    """No Rewind in a 2P VS (split-screen) race; Rewind still works in 1P."""
    vs_slot = int(opt(args, "--vs-slot", 9))
    grid_slot = int(opt(args, "--grid-slot", 7))
    rewind_keys = ["select", "triangle"] if "--classic" in args else ["triangle"]
    report = {"checks": []}
    wait_up()
    c({"cmd": "turbo", "enabled": 0})

    restore(grid_slot)
    host(rt=255)
    wait_frames(30)
    a = rewind_status()
    wait_frames(120)
    b = rewind_status()
    check(report, "1P race: Rewind enabled, not blocked, capturing",
          a["enabled"] and not b["title_blocked"] and b["snaps"] > a["snaps"],
          before=a, after=b)

    restore(vs_slot)
    host(rt=255)
    wait_frames(30)
    a = rewind_status()
    wait_frames(300)
    b = rewind_status()
    check(report, "2P VS race: Rewind blocked, no history captured in 300 frames",
          a["title_blocked"] and b["title_blocked"] and b["snaps"] == a["snaps"],
          before=a, after=b)
    advanced, opened = press_and_watch(["triangle"])
    check(report, "2P VS race: Y does not open Rewind (guest keeps running)",
          advanced > 0 and not opened, frames_advanced=advanced, opened=opened)
    wait_frames(30)
    advanced, opened = press_and_watch(["select", "triangle"])
    check(report, "2P VS race: Select + Y does not open Rewind either",
          advanced > 0 and not opened, frames_advanced=advanced, opened=opened)
    wait_frames(30)
    b2 = rewind_status()
    check(report, "2P VS race: still no history after the presses",
          b2["snaps"] == a["snaps"] and not b2["open"], status=b2)

    restore(grid_slot)
    host(rt=255)
    wait_frames(30)
    a = rewind_status()
    wait_frames(120)
    b = rewind_status()
    check(report, "back in 1P: unblocked and capturing again",
          not a["title_blocked"] and b["snaps"] > a["snaps"], before=a, after=b)
    advanced, opened = press_and_watch(rewind_keys)
    host()
    time.sleep(0.2)
    host(["circle"])
    time.sleep(0.2)
    host()
    time.sleep(0.3)
    f3 = frame()
    time.sleep(0.5)
    closed = not rewind_status()["open"]
    check(report, "back in 1P: " + " + ".join(rewind_keys) +
          " opens Rewind (guest frozen), Circle cancels",
          opened and advanced == 0 and closed and frame() > f3,
          frames_advanced_while_open=advanced, opened=opened, closed=closed)
    c({"cmd": "clear_input"})
    m = misses()
    check(report, "0 dispatch and 0 segment misses",
          m["dispatch"] == 0 and m["segment"] == 0, **m)
    return 0 if all(x["ok"] for x in report["checks"]) else 1


# ---- sio ------------------------------------------------------------------

# Host script (frames, buttons, lx, lt, rt): every Modern-relevant input.
SIO_SCRIPT = [
    (60, (), 128, 0, 255), (30, (), 0, 0, 255), (30, (), 255, 128, 128),
    (20, ("square",), 128, 0, 255), (20, ("circle",), 128, 0, 255),
    (20, ("r1",), 128, 0, 255), (20, ("triangle",), 128, 0, 255),
    (60, (), 96, 255, 0), (30, (), 128, 0, 0),
]


def sio_polls(entries):
    """Group raw SIO bytes into polls; keep P1 controller polls only."""
    polls, cur = [], None
    for e in entries:
        tx, rx = int(e["tx"], 16), int(e["rx"], 16)
        port = (int(e["ctrl"], 16) >> 13) & 1
        if tx == 0x01:
            if cur:
                polls.append(cur)
            cur = {"port": port, "rx": [rx]} if port == 0 else None
            continue
        if cur is not None:
            cur["rx"].append(rx)
    # The trace can end mid-poll: the last poll is left out.
    return [tuple(p["rx"]) for p in polls]


def run_length(seq):
    out = []
    for item in seq:
        if out and out[-1][0] == item:
            out[-1][1] += 1
        else:
            out.append([item, 1])
    return out


def sio_trace(count):
    """sio_trace answers on several lines; parse the whole reply."""
    return json.loads(send(json.dumps({"cmd": "sio_trace", "count": count}),
                           port=PORT, timeout=120))


def cmd_sio(args):
    out = args[0]
    slot = int(opt(args, "--slot", 9))
    wait_up()
    c({"cmd": "turbo", "enabled": 0})
    restore(slot)
    start = sio_trace(1)["total"]
    for frames, buttons, lx, lt, rt in SIO_SCRIPT:
        host(buttons, lx=lx, lt=lt, rt=rt)
        wait_frames(frames)
    host()
    end = sio_trace(1)["total"]
    raw = sio_trace(end - start)
    polls = sio_polls(raw["entries"])
    with open(out, "w", encoding="utf-8") as f:
        json.dump({"polls": len(polls), "runs": run_length(polls),
                   "misses": misses(), "car": car()}, f)
    print(json.dumps({"out": out, "polls": len(polls),
                      "distinct": len({p for p in polls}), "misses": misses()}))
    return 0


def cmd_compare(args):
    a, b = (json.load(open(p, encoding="utf-8")) for p in args[:2])
    ra = [tuple(x[0]) for x in a["runs"]]
    rb = [tuple(x[0]) for x in b["runs"]]
    same = ra == rb
    print(json.dumps({"identical_poll_sequence": same, "polls": [a["polls"], b["polls"]],
                      "segments": [len(ra), len(rb)],
                      "segment_lengths_equal": [x[1] for x in a["runs"]] ==
                                               [x[1] for x in b["runs"]]}))
    return 0 if same else 1


def opt(args, name, default):
    return args[args.index(name) + 1] if name in args else default


def main():
    global PORT
    args = sys.argv[1:]
    if "--port" in args:
        PORT = int(opt(args, "--port", PORT))
    if not args:
        print(__doc__)
        return 2
    sub, rest = args[0], args[1:]
    return {"grid": cmd_grid, "modern": cmd_modern, "classic": cmd_classic,
            "splitscreen": cmd_splitscreen, "sio": cmd_sio,
            "compare": cmd_compare}[sub](rest)


if __name__ == "__main__":
    try:
        sys.exit(main())
    finally:
        try:
            cmd({"cmd": "clear_input"}, port=PORT, timeout=2)
        except Exception:
            pass

#!/usr/bin/env python3
"""smoke.py - scripted boot-to-race check against a running debug build.

Drives pad 1 over the debug server with frontend turbo on (unthrottled), so a
boot-to-race run takes seconds of wall clock: skips the intro, opens Grand
Prix, confirms through the pre-race screens, then holds accelerate. All timing
is in guest frames. Writes a numbered PNG per step into OUT_DIR and prints
frame, dispatch-miss and segment-miss counters, so a run can be judged from
the images and the numbers alone.

Usage: python3 tools/smoke.py OUT_DIR [--port 4797]
Start the game first, e.g.:  tools/run_r4.sh build
"""
import os
import sys
import time

from dbg import DEFAULT_PORT, cmd
from pad import mask

# (label, buttons held, hold secs, settle secs) -- secs are guest time
# (x60 frames). R4 menus use Circle = OK.
# The Namco logo is unskippable; Start skips the intro movie once it runs
# (START_FRAME), then opens the menu, then picks Grand Prix.
START_FRAME = 1800
STEPS = [
    ("intro", None, 0, 0),
    ("skip-intro", ["start"], 0.6, 4),
    ("menu", ["start"], 0.3, 3),
    ("gp-select", ["start"], 0.3, 4),
]
# First-visit Grand Prix briefing auto-advances (Circle disabled) and is then
# followed by team / maker / car select, where Circle = OK. Alternating
# Circle and Start walks both kinds of screen; after the heat overview only
# Circle is safe (Start pauses once the race is live).
STEPS += [(f"gp-{i:02d}", ["circle" if i % 2 == 0 else "start"], 0.2, 2.3) for i in range(22)]
STEPS += [(f"pre-{i}", ["circle"], 0.2, 2.8) for i in range(10)]
STEPS += [
    ("race-3s", ["cross"], 3, 0),
    ("race-6s", ["cross"], 3, 0),
    ("race-9s", ["cross", "left"], 1.5, 0),
    ("race-12s", ["cross"], 3, 0),
]


def wait_frames(n, port):
    target = cmd({"cmd": "frame"}, port=port).get("frame", 0) + n
    while cmd({"cmd": "frame"}, port=port).get("frame", 0) < target:
        time.sleep(0.02)


def engage_widescreen(port):
    """Let frames present on a 2D screen, then resume turbo.

    TCP turbo skips the whole present path, and psxrecomp engages a wide
    display aspect (the widescreen mod) in that path, on the first frame it
    presents after game entry (never during an FMV, and not while the
    post-FMV present hold is still draining). Turbo switched on during boot
    would otherwise keep the whole run 4:3. Waits until native-wide engages,
    or at most 60 frames when no wide aspect is configured.
    """
    cmd({"cmd": "turbo", "enabled": 0}, port=port)
    mode = 0
    for _ in range(60):
        wait_frames(1, port)
        mode = cmd({"cmd": "ws_nw"}, port=port).get("mode")
        if mode == 2:
            break
    cmd({"cmd": "turbo", "enabled": 1}, port=port)
    print(f"   widescreen mode={mode} (0 off, 2 native-wide)")


def main():
    out = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "smoke")
    port = int(sys.argv[sys.argv.index("--port") + 1]) if "--port" in sys.argv else DEFAULT_PORT
    os.makedirs(out, exist_ok=True)
    deadline = time.time() + 60
    while True:
        try:
            cmd({"cmd": "turbo", "enabled": 1}, port=port, timeout=2)
            break
        except OSError:
            if time.time() > deadline:
                print("debug server not reachable (build with -DPSX_DEBUG_TOOLS=ON)")
                return 1
            time.sleep(0.5)
    wait_frames(max(0, START_FRAME - cmd({"cmd": "frame"}, port=port).get("frame", 0)), port)
    try:
        for i, (label, buttons, hold, settle) in enumerate(STEPS):
            if buttons:
                cmd({"cmd": "set_input", "buttons": "0x%04X" % mask(buttons)}, port=port)
                wait_frames(int(hold * 60), port)
                if not label.startswith("race"):
                    cmd({"cmd": "clear_input"}, port=port)
            wait_frames(int(settle * 60), port)
            if label == "menu":
                engage_widescreen(port)
            path = os.path.join(out, f"{i:02d}_{label}.png")
            cmd({"cmd": "screenshot", "path": path}, port=port)
            frame = cmd({"cmd": "frame"}, port=port).get("frame")
            d = cmd({"cmd": "dispatch_stats"}, port=port)
            print(f"{i:02d} {label:<11} frame={frame} "
                  f"miss_total={d.get('miss_total')} miss_unique={d.get('miss_unique')} "
                  f"seg_miss={d.get('segment_miss_total')}")
    finally:
        cmd({"cmd": "clear_input"}, port=port)
        cmd({"cmd": "turbo", "enabled": 0}, port=port)
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""pad.py - drive pad 1 over the debug server and capture screenshots.

Usage: python3 tools/pad.py [--port 4797] STEP [STEP ...]
Steps:
  <buttons>[:secs]   hold buttons (comma list) for secs (default 0.15), then release
  wait:<secs>        idle
  shot:<path>        screenshot (PNG, native display)
Buttons: select l3 r3 start up right down left l2 r2 l1 r1 triangle circle cross square
Example (skip intro, open menu, capture):
  python3 tools/pad.py start wait:2 start wait:3 shot:/tmp/menu.png
"""
import sys
import time

from dbg import DEFAULT_PORT, cmd

# PS1 digital pad bit order (active-low on the wire).
BITS = ["select", "l3", "r3", "start", "up", "right", "down", "left",
        "l2", "r2", "l1", "r1", "triangle", "circle", "cross", "square"]


def mask(names):
    m = 0xFFFF
    for n in names:
        m &= ~(1 << BITS.index(n))
    return m


def hold(names, secs, port):
    cmd({"cmd": "set_input", "buttons": "0x%04X" % mask(names)}, port=port)
    time.sleep(secs)
    cmd({"cmd": "clear_input"}, port=port)


def main():
    args = sys.argv[1:]
    port = DEFAULT_PORT
    if args[:1] == ["--port"]:
        port = int(args[1])
        args = args[2:]
    if not args:
        print(__doc__)
        return 2
    for step in args:
        head, _, arg = step.partition(":")
        if head == "wait":
            time.sleep(float(arg))
        elif head == "shot":
            r = cmd({"cmd": "screenshot", "path": arg}, port=port)
            print(f"shot {arg} ok={r.get('ok')} frame={cmd({'cmd': 'frame'}, port=port).get('frame')}")
        else:
            hold(head.split(","), float(arg) if arg else 0.15, port)
            time.sleep(0.1)
    return 0


if __name__ == "__main__":
    sys.exit(main())

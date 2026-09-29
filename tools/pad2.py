#!/usr/bin/env python3
"""pad2.py - hold pad-2 buttons for N guest frames (2P VS testing only).

The debug server injects pad 1 only. With `[controller] p2_device = "gamepad"`
in build/settings.toml and no controller attached, port 2 reports a connected,
idle pad, which is enough to enable VS Battle; this script then presses its
buttons by writing the button bytes of R4's two port-2 receive buffers once per
guest frame (0x8010C2FA/FB and 0x8011459D/9E, active-low pad word, low byte
first; pad 1's are at 0x8010C2D2 and 0x8011457A). The debug server services
commands at the frame boundary, and the game reads the written value.

It writes guest RAM, so use it for captures, never for identity checks.

Usage: python3 tools/pad2.py [--port 4797] BUTTON[,BUTTON] FRAMES
  python3 tools/pad2.py circle 12      # confirm Car Select Preset Player 2
  python3 tools/pad2.py cross 1500     # accelerate
Buttons as in tools/pad.py.
"""
import sys
import time

from dbg import DEFAULT_PORT, cmd
from pad import mask

PAD2_BUTTON_BYTES = (0x8010C2FA, 0x8011459D)


def main():
    args = sys.argv[1:]
    port = DEFAULT_PORT
    if args[:1] == ["--port"]:
        port = int(args[1])
        args = args[2:]
    if len(args) != 2:
        print(__doc__)
        return 2
    word = mask(args[0].split(","))
    frames = int(args[1])
    start = cmd({"cmd": "frame"}, port=port).get("frame", 0)
    last = -1
    while True:
        frame = cmd({"cmd": "frame"}, port=port).get("frame", 0)
        if frame - start >= frames:
            break
        if frame != last:
            for addr in PAD2_BUTTON_BYTES:
                cmd({"cmd": "write_ram", "addr": hex(addr), "val": word & 0xFF},
                    port=port)
                cmd({"cmd": "write_ram", "addr": hex(addr + 1),
                     "val": (word >> 8) & 0xFF}, port=port)
            last = frame
        time.sleep(0.001)
    print(f"pad 2 held 0x{word:04X} for {frame - start} frames")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Read the recomp/oracle stack and print return-address candidates.

Usage: python3 tools/stackwalk.py [port] [base] [len]
R4's CRT sets sp = 0x80200000 (not the header s_addr), so the live stack
grows down from there. Candidates cover game text and the overlay window
(R4.BIN overlays are linked at 0x801149A8).
"""
import sys

from dbg import cmd

port = int(sys.argv[1]) if len(sys.argv) > 1 else 4797
base = int(sys.argv[2], 0) if len(sys.argv) > 2 else 0x801FFE00
length = int(sys.argv[3], 0) if len(sys.argv) > 3 else 0x200

TEXT = (0x80010000, 0x800AC000)
OVERLAY = (0x801149A8, 0x801281A8)

r = cmd({"cmd": "read_ram", "addr": f"0x{base:08X}", "len": length}, port=port)
b = bytes.fromhex(r.get("hex", ""))
print(f"stack {hex(base)}..{hex(base + length)} return-addr candidates:")
for i in range(0, len(b) - 3, 4):
    w = int.from_bytes(b[i:i + 4], "little")
    if TEXT[0] <= w < TEXT[1] or OVERLAY[0] <= w < OVERLAY[1]:
        print(f"  [0x{base + i:08X}] = 0x{w:08X}")

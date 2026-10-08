#!/usr/bin/env python3
"""Verify the US R4 link-input evidence and print a local hook-word manifest.

Reads retail assets supplied by the owner. It does not patch either asset or
include their instruction words in the source tree.
"""

import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys


EXE_BASE = 0x80010000
EXE_HEADER_SIZE = 0x800
OVERLAY_BASE = 0x801149A8
SECTOR_SIZE = 0x800
EXE_SHA256 = "078e3a95185cfa0cc79936dce37cc2c1a90e17aa209b42eef342c970a97f3323"
OVERLAY_SHA256 = "bf1c8c52e0a19609ccbcc0ee9989cb7307bb9b6bc4a1fc64b137bb5bdfb75eae"

# Addresses are data, not copied instructions. Keep this list narrow: each
# entry is a boundary or call site needed for an eventual four-input proof.
SITES = {
    "exe": {
        "command_builder": 0x8002961C,
        "mode_gate": 0x800296AC,
        "link_selection": 0x80029C3C,
        "remote_command_base": 0x80029C5C,
        "link_selection_end": 0x80029CAC,
        "four_car_roster": 0x80037A14,
        "mode4_car_loop": 0x800383E8,
        "call_command_builder": 0x80038410,
        "call_car_update": 0x80038468,
        "car_command_read": 0x80022D60,
        "viewport_setup": 0x8006F2B0,
        "sio1_register_base": 0x800ABED0,
    },
    "overlay": {
        "send_record": 0x80114E60,
        "copy_local_commands": 0x80114EBC,
        "call_serial_send": 0x80114F38,
        "receive_record": 0x80114F64,
        "consume_ring": 0x80115198,
        "copy_remote_commands": 0x8011527C,
        "link_error_exit": 0x80115520,
        "link_race_handler": 0x80115770,
        "view_loop": 0x80115F84,
        "call_car_draw": 0x801160E0,
        "serial_init": 0x80114C28,
        "serial_setup_poll": 0x80114D94,
        "serial_handshake": 0x801155C4,
        "serial_handshake_poll": 0x801155DC,
    },
}


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def checked_word(data, offset, label):
    if offset < 0 or offset + 4 > len(data):
        raise ValueError(f"{label}: word lies outside input")
    return struct.unpack_from("<I", data, offset)[0]


def overlay_entry(archive, index):
    count = checked_word(archive, 0, "R4.BIN count")
    if count <= 672 or 4 + count * 4 > len(archive):
        raise ValueError("R4.BIN index table is too short")
    sector = checked_word(archive, 4 + 4 * index, f"entry {index} sector")
    next_sector = checked_word(archive, 4 + 4 * (index + 1), f"entry {index + 1} sector")
    start, end = sector * SECTOR_SIZE, next_sector * SECTOR_SIZE
    if not (0 <= start < end <= len(archive)):
        raise ValueError(f"entry {index} has invalid sector bounds")
    return archive[start:end], start


def jal_target(address, word):
    if word >> 26 != 3:
        raise ValueError(f"0x{address:08X}: expected JAL")
    return ((address + 4) & 0xF0000000) | ((word & 0x03FFFFFF) << 2)


def build_manifest(exe, archive):
    if exe[:8] != b"PS-X EXE" or checked_word(exe, 0x18, "EXE base") != EXE_BASE:
        raise ValueError("expected US PS-X EXE with text base 0x80010000")
    if sha256(exe) != EXE_SHA256:
        raise ValueError("EXE SHA-256 differs from the authenticated SLUS_007.97")
    overlay, offset = overlay_entry(archive, 667)
    alias, _ = overlay_entry(archive, 672)
    if overlay != alias or len(overlay) != 18432 or sha256(overlay) != OVERLAY_SHA256:
        raise ValueError("R4.BIN entries 667/672 differ from the authenticated link overlay")

    words = {}
    for source, sites in SITES.items():
        words[source] = {}
        for name, address in sites.items():
            file_offset = (EXE_HEADER_SIZE + address - EXE_BASE) if source == "exe" else (offset + address - OVERLAY_BASE)
            data = exe if source == "exe" else archive
            words[source][name] = {
                "address": f"0x{address:08X}",
                "file_offset": f"0x{file_offset:X}",
                "word": f"0x{checked_word(data, file_offset, name):08X}",
            }

    for source, name, target in (
        ("exe", "call_command_builder", 0x8002961C),
        ("exe", "call_car_update", 0x80022B20),
        ("overlay", "call_serial_send", 0x80097130),
        ("overlay", "call_car_draw", 0x8002E554),
        ("overlay", "serial_setup_poll", 0x8009B088),
        ("overlay", "serial_handshake_poll", 0x8009B088),
    ):
        item = words[source][name]
        actual = jal_target(int(item["address"], 16), int(item["word"], 16))
        if actual != target:
            raise ValueError(f"{name}: call target 0x{actual:08X}, expected 0x{target:08X}")
    if int(words["exe"]["sio1_register_base"]["word"], 16) != 0x1F801050:
        raise ValueError("game serial helper no longer points at SIO1")

    return {
        "exe_sha256": EXE_SHA256,
        "overlay_667_672_sha256": OVERLAY_SHA256,
        "overlay_667_size": len(overlay),
        "sites": words,
        "status": "static authenticated words and four call targets; no runtime proof",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True, help="local SLUS_007.97")
    parser.add_argument("--r4-bin", type=Path, required=True, help="local R4.BIN archive")
    args = parser.parse_args()
    try:
        manifest = build_manifest(args.exe.read_bytes(), args.r4_bin.read_bytes())
    except (OSError, ValueError) as exc:
        parser.error(str(exc))
    json.dump(manifest, sys.stdout, indent=2)
    print()


if __name__ == "__main__":
    main()

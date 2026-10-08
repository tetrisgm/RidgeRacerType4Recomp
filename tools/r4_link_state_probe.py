#!/usr/bin/env python3
"""Offline R4 forced-mode diagnostic using a copied, matching VS savestate.

`prepare` writes a patched copy of a local savestate. `verify` loads that copy
twice in one matching debug build; `verify-peers` loads it in two independent
matching debug builds. Both compare guest fingerprints after each load. Set
PSX_DEBUG_FMV_QUIET=0 in both processes for a cold two-process comparison, so
startup media classification cannot change fingerprint column accounting.
This experiment changes the mode/count fields inside a VS race. The active
handler remains VS; this cannot prove link command consumption or moving
four-car gameplay. It only checks what the forced state actually does.
"""

import argparse
import hashlib
import json
from pathlib import Path
import struct
import time
import zlib

from dbg import cmd

MODE_ADDR = 0x800F4EF4
COUNT_ADDR = 0x800AC754
SIDE_ADDR = 0x800AD6C0
LOCAL_ADDR = 0x800ACDA8
REMOTE_ADDR = 0x800ACD98
OUTPUT_ADDR = 0x800BDC88
POINTERS_ADDR = 0x800FFDD0
HANDLER_ADDR = 0x800F4E1A
HANDLER_SUBINDEX_ADDR = 0x800F3BD6
HANDLER_TABLE_ADDR = 0x8009EBCC
CAR_BYTES = 0x320
RAM_TAG = 2
HEADER_BYTES = 36


def parse_hex_commands(raw):
    try:
        value = bytes.fromhex(raw)
    except ValueError as exc:
        raise SystemExit(f"invalid --commands: {exc}") from exc
    if len(value) != 16 or len(set(value[i:i + 4] for i in range(0, 16, 4))) != 4:
        raise SystemExit("--commands requires four distinct four-byte words")
    return value


def ram_section(blob):
    if len(blob) < HEADER_BYTES:
        raise ValueError("truncated savestate header")
    fields = struct.unpack_from('<9I', blob)
    if fields[0] != 0x50535842:
        raise ValueError("not a PSXB savestate")
    count = fields[7]
    pos = HEADER_BYTES
    for _ in range(count):
        if pos + 16 > len(blob):
            raise ValueError("truncated savestate section")
        tag, flags, size = struct.unpack_from('<IIQ', blob, pos)
        pos += 16
        if pos + size > len(blob):
            raise ValueError("truncated savestate payload")
        payload = blob[pos:pos + size]
        pos += size
        if tag == RAM_TAG:
            if flags & 1:
                size_raw, = struct.unpack_from('<I', payload)
                data = zlib.decompress(payload[4:])
                if len(data) != size_raw:
                    raise ValueError("savestate RAM size mismatch")
            else:
                data = payload
            yield pos - size - 16, pos, tag, flags, data
    if pos != len(blob):
        raise ValueError("unexpected trailing savestate bytes")


def prepare(source, dest, commands):
    if source.resolve() == dest.resolve():
        raise ValueError("destination must differ from source")
    blob = source.read_bytes()
    sections = list(ram_section(blob))
    if len(sections) != 1:
        raise ValueError(f"expected one main-RAM section, got {len(sections)}")
    begin, end, tag, flags, source_ram = sections[0]
    if len(source_ram) != 2 * 1024 * 1024:
        raise ValueError(f"expected retail 2 MiB RAM, got {len(source_ram)}")
    ram = bytearray(source_ram)
    def get16(addr):
        return struct.unpack_from('<H', ram, addr & 0x1fffff)[0]
    if get16(MODE_ADDR) != 2 or get16(COUNT_ADDR) != 2:
        raise ValueError("source is not a two-car mode-2 VS state")
    if get16(SIDE_ADDR) == 0:
        raise ValueError("source uses the reversed link side; command ordering needs a separate fixture")
    pointers = struct.unpack_from('<4I', ram, POINTERS_ADDR & 0x1fffff)
    if len(set(pointers)) != 4 or any(not 0x80000000 <= p < 0x80200000 for p in pointers):
        raise ValueError("source lacks four distinct valid car pointers")
    struct.pack_into('<H', ram, MODE_ADDR & 0x1fffff, 4)
    struct.pack_into('<H', ram, COUNT_ADDR & 0x1fffff, 4)
    ram[LOCAL_ADDR & 0x1fffff:(LOCAL_ADDR & 0x1fffff) + 8] = commands[:8]
    ram[REMOTE_ADDR & 0x1fffff:(REMOTE_ADDR & 0x1fffff) + 8] = commands[8:]
    if flags & 1:
        payload = struct.pack('<I', len(ram)) + zlib.compress(ram, level=1)
    else:
        payload = bytes(ram)
    replacement = struct.pack('<IIQ', tag, flags, len(payload)) + payload
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(blob[:begin] + replacement + blob[end:])
    return {'source_sha256': hashlib.sha256(blob).hexdigest(),
            'output_sha256': hashlib.sha256(dest.read_bytes()).hexdigest(),
            'car_pointers': [hex(p) for p in pointers]}


def verify(ports, slot, commands, min_frames):
    def ask(port, name, **kw):
        reply = cmd({'cmd': name, **kw}, port=port, timeout=20)
        if not reply.get('ok'):
            raise RuntimeError((name, reply))
        return reply
    def read(port, addr, length):
        return bytes.fromhex(ask(port, 'read_ram', addr=hex(addr), len=length)['hex'])
    def frame(port):
        return ask(port, 'frame')['frame']
    def replay(port):
        before = ask(port, 'savestate_status')['generation']
        ask(port, 'frame_fingerprint', reset_on_load=1)
        ask(port, 'savestate', op='load', slot=slot)
        for _ in range(200):
            status = ask(port, 'savestate_status')
            if status['generation'] > before and not status['pending']:
                if not status['last_ok']:
                    raise RuntimeError(('savestate load failed', status))
                break
            time.sleep(0.01)
        else:
            raise RuntimeError('savestate load timeout')
        start = frame(port)
        pointers = struct.unpack('<4I', read(port, POINTERS_ADDR, 16))
        xz_before = [struct.unpack_from('<i', data, 0)[0:1] +
                     struct.unpack_from('<i', data, 8)[0:1]
                     for data in (read(port, p, 12) for p in pointers)]
        while frame(port) < start + min_frames:
            time.sleep(0.005)
        fp = ask(port, 'frame_fingerprint', count=min_frames + 100)['entries']
        xz_after = [struct.unpack_from('<i', data, 0)[0:1] +
                    struct.unpack_from('<i', data, 8)[0:1]
                    for data in (read(port, p, 12) for p in pointers)]
        handler_selector, = struct.unpack('<H', read(port, HANDLER_ADDR, 2))
        handler_subindex, = struct.unpack('<H', read(port, HANDLER_SUBINDEX_ADDR, 2))
        handler_table, = struct.unpack(
            '<I', read(port, HANDLER_TABLE_ADDR + 4 * handler_selector, 4))
        handler_pointer, = struct.unpack(
            '<I', read(port, handler_table + 4 * handler_subindex, 4))
        return {'start': start, 'fingerprints': fp,
                'mode': read(port, MODE_ADDR, 2).hex(),
                'count': read(port, COUNT_ADDR, 2).hex(),
                'side': read(port, SIDE_ADDR, 2).hex(),
                'input_buffers': (read(port, LOCAL_ADDR, 8) +
                                  read(port, REMOTE_ADDR, 8)).hex(),
                'output_commands': read(port, OUTPUT_ADDR, 16).hex(),
                'handler_selector': handler_selector,
                'handler_subindex': handler_subindex,
                'handler_pointer': hex(handler_pointer),
                'position_deltas': [[after[0] - before[0], after[1] - before[1]]
                                    for before, after in zip(xz_before, xz_after)],
                'car_pointers': [hex(p) for p in pointers],
                'car_hashes': [hashlib.sha256(read(port, p, CAR_BYTES)).hexdigest() for p in pointers],
                'dispatch': ask(port, 'dispatch_stats')}
    runs = [replay(port) for port in ports]
    for run in runs:
        if run['mode'] != '0400' or run['count'] != '0400':
            raise RuntimeError(('not in four-car mode', run['mode'], run['count']))
        if run['side'] == '0000':
            raise RuntimeError('link side changed from the prepared fixture')
        if run['input_buffers'] != commands.hex():
            raise RuntimeError(('input buffers changed', run['input_buffers'], commands.hex()))
        if len(set(run['car_pointers'])) != 4:
            raise RuntimeError('four car pointers not distinct')
        if run['dispatch']['miss_total'] or run['dispatch'].get('segment_miss_total', 0):
            raise RuntimeError(('dispatch miss', run['dispatch']))
    a, b = [run['fingerprints'] for run in runs]
    comparable = min(len(a), len(b))
    if comparable < min_frames:
        raise RuntimeError(('too few comparable frames', comparable))
    for i in range(comparable):
        left = {k: v for k, v in a[i].items() if k != 'frame'}
        right = {k: v for k, v in b[i].items() if k != 'frame'}
        if left != right:
            raise RuntimeError(('guest fingerprint divergence', i, left, right))
    return {'comparable_frames': comparable,
            'first_frame_each': [run['fingerprints'][0]['frame'] for run in runs],
            'car_pointers': runs[0]['car_pointers'],
            'injected_input_buffers': runs[0]['input_buffers'],
            'output_commands': runs[0]['output_commands'],
            'handler_selector': runs[0]['handler_selector'],
            'handler_subindex': runs[0]['handler_subindex'],
            'handler_pointer': runs[0]['handler_pointer'],
            'position_deltas': runs[0]['position_deltas'],
            'zero_misses': True,
            'fingerprints_identical': True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='action', required=True)
    prep = sub.add_parser('prepare')
    prep.add_argument('--source', type=Path, required=True)
    prep.add_argument('--output', type=Path, required=True)
    prep.add_argument('--commands', required=True,
                      help='16-byte hex string: four distinct observed command words')
    check = sub.add_parser('verify')
    check.add_argument('--port', type=int, required=True)
    check.add_argument('--slot', type=int, required=True)
    check.add_argument('--commands', required=True)
    check.add_argument('--frames', type=int, default=180)
    peers = sub.add_parser('verify-peers')
    peers.add_argument('--port-a', type=int, required=True)
    peers.add_argument('--port-b', type=int, required=True)
    peers.add_argument('--slot', type=int, required=True)
    peers.add_argument('--commands', required=True)
    peers.add_argument('--frames', type=int, default=180)
    args = parser.parse_args()
    commands = parse_hex_commands(args.commands)
    if args.action == 'prepare':
        result = prepare(args.source, args.output, commands)
    elif args.action == 'verify':
        result = verify((args.port, args.port), args.slot, commands, args.frames)
    else:
        result = verify((args.port_a, args.port_b), args.slot, commands, args.frames)
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()

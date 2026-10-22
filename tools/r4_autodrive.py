#!/usr/bin/env python3
"""r4_autodrive.py - closed-loop driver for timing runs on a debug build.

Steers pad 1 along the course centreline read from guest RAM, so a test lap
stays on the track whatever the build's guest state (an open-loop input
route desyncs as soon as draw distance, heaps or timing change and drives
the car into a wall). Holds accelerate; steers toward the centreline point
LOOKAHEAD segments ahead of the nearest one. Racing runs toward lower
segment numbers (src/mods/r4_pvs.h). The camera position (scratchpad
0x1F800008, /4 = centreline units) stands in for the car.

Usage: python tools/r4_autodrive.py SECONDS [--port 4797] [--record ROUTE]
--record writes the applied buttons as an input route (frames hex-buttons,
the PSX_INPUT_REPLAY format) for replay on a production build of the same
branch. Prints progress (segment, speed) once a second.
"""
import argparse
import math
import time

from dbg import DEFAULT_PORT, cmd

CAM = 0x1F800008
SEG_TABLE = 0x800AC05C
SEG_COUNT = 0x800AC084
SEG_STRIDE = 0x3C
LOOKAHEAD = 4
ACCEL, RIGHT, LEFT = 0xBFFF, 0xBFDF, 0xBF7F
DEADZONE = 0.04   # radians


def s32(v):
    return v - (1 << 32) if v >= 1 << 31 else v


def words(addr, n, port):
    r = cmd({'cmd': 'mem_words', 'addr': hex(addr), 'count': n}, port=port)
    return [int(x, 16) for x in r['words']]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('seconds', type=float)
    p.add_argument('--port', type=int, default=DEFAULT_PORT)
    p.add_argument('--record')
    a = p.parse_args()

    n = words(SEG_COUNT, 1, a.port)[0]
    tab = words(SEG_TABLE, 1, a.port)[0]
    segs = []
    for i in range(0, n, 16):
        w = words(tab + SEG_STRIDE * i, SEG_STRIDE // 4 * min(16, n - i), a.port)
        for k in range(min(16, n - i)):
            segs.append((s32(w[k * 15 + 2]), s32(w[k * 15 + 3])))

    def cam():
        x, _, z = (s32(v) for v in words(CAM, 3, a.port))
        return x / 4.0, z / 4.0

    def nearest(pos, hint):
        rng = range(n) if hint is None else (i % n for i in range(hint - 20, hint + 21))
        return min(rng, key=lambda i: (segs[i][0] - pos[0]) ** 2 + (segs[i][1] - pos[1]) ** 2)

    route, last_btn, run_start = [], None, None
    f0 = cmd({'cmd': 'frame'}, port=a.port)['frame']
    prev, seg, end = cam(), None, time.monotonic() + a.seconds
    next_report = time.monotonic() + 1
    while time.monotonic() < end:
        pos = cam()
        seg = nearest(pos, seg)
        tx, tz = segs[(seg - LOOKAHEAD) % n]
        hx, hz = pos[0] - prev[0], pos[1] - prev[1]
        if hx * hx + hz * hz < 1.0:   # barely moving: aim along the track
            ax, az = segs[(seg - 1) % n]
            hx, hz = ax - segs[seg][0], az - segs[seg][1]
        want = math.atan2(tx - pos[0], tz - pos[1])
        have = math.atan2(hx, hz)
        err = (want - have + math.pi) % (2 * math.pi) - math.pi
        btn = ACCEL if abs(err) < DEADZONE else (RIGHT if err > 0 else LEFT)
        frame = cmd({'cmd': 'frame'}, port=a.port)['frame']
        if btn != last_btn:
            cmd({'cmd': 'set_input', 'buttons': hex(btn)}, port=a.port)
            if last_btn is not None:
                route.append((frame - run_start, last_btn))
            last_btn, run_start = btn, frame
        prev = pos
        if time.monotonic() >= next_report:
            print(f'frame={frame} seg={seg} step={math.hypot(hx, hz):.1f}', flush=True)
            next_report += 1
        time.sleep(1 / 60)
    frame = cmd({'cmd': 'frame'}, port=a.port)['frame']
    route.append((frame - run_start, last_btn))
    cmd({'cmd': 'clear_input'}, port=a.port)
    if a.record:
        with open(a.record, 'w') as f:
            f.write(f'# applied digital pad route from r4_autodrive.py, start frame {f0}\n')
            f.writelines(f'{d} {b:04x}\n' for d, b in route if d > 0)


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Bounded TCP measurements; launch exactly one runtime directly from the shell.

Identity: launch fullscreen, select the mod before launch, run twice with the
same build/settings/state, then compare. Identity runs at native cadence. An on
receipt must contain completed temporal passes; headless cannot prove this.
Timing: launch windowed with PSX_RUNTIME_PERF_DIAG=1 and
PSX_RUNTIME_PERF_DIAG_MS=250; the probe sets a 960x540 window.
"""
import argparse
import hashlib
import json
import os
import time
from pathlib import Path

from dbg import cmd

FIELDS = ('cyc', 'mmio', 'mc', 'sp', 'sc', 'wc', 'ws', 'qc')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=('identity', 'timing', 'compare'))
    parser.add_argument('output', type=Path)
    parser.add_argument('--port', type=int, default=4937)
    parser.add_argument('--slot', type=int, default=2)
    parser.add_argument('--frames', type=int, default=61)
    parser.add_argument('--seconds', type=float, default=30)
    parser.add_argument('--warmup-frames', type=int, default=900,
                        help='replay this many frames in turbo before a timing window')
    parser.add_argument('--route', type=Path, required=False,
                        help='recorded applied pad inputs (pc-record.json) paired with the loaded slot')
    parser.add_argument('--sample-passes', action='store_true',
                        help='poll GL telemetry (forces render-thread drains; do not use for base timing)')
    parser.add_argument('--sample-threads', action='store_true',
                        help='sample non-draining render/presenter wait counters')
    parser.add_argument('--inputs', nargs=2, type=Path)
    args = parser.parse_args()
    if args.mode != 'compare' and not args.route:
        parser.error('race runs require --route and its matching savestate slot')
    if args.mode == 'timing' and not 30 <= args.seconds <= 60:
        parser.error('timing windows must be between 30 and 60 seconds')
    if args.output.exists():
        parser.error('output already exists; use a new receipt name')
    if args.mode == 'compare':
        if not args.inputs:
            parser.error('compare requires --inputs OFF ON')
        runs = [json.loads(p.read_text()) for p in args.inputs]
        a, b = [r['fingerprint']['entries'] for r in runs]
        bad = []
        for i, (x, y) in enumerate(zip(a, b)):
            diff = {k: [x[k], y[k]] for k in FIELDS if x[k] != y[k]}
            # The frontend frame counter is host history, not savestate state.
            # Align by offset from the first post-load fingerprint instead.
            offsets = [x['frame'] - a[0]['frame'], y['frame'] - b[0]['frame']]
            if offsets[0] != offsets[1]:
                diff['frame_offset'] = offsets
            if diff:
                bad.append(dict(index=i, diff=diff))
        result = dict(lengths=[len(a), len(b)], compared=min(len(a), len(b)),
                      mismatches=len(bad), first=bad[:3],
                      passes=[r['render_pass_stats']['passes'] for r in runs])
        result['ok'] = (len(a) == len(b) > 0 and not bad and
                        runs[0]['render_pass_stats']['passes'] == 0 and
                        runs[1]['render_pass_stats']['passes'] > 0)
    else:
        def ask(name, **kw):
            reply = cmd(dict(cmd=name, **kw), port=args.port, timeout=10)
            if not reply or not reply.get('ok', True):
                raise RuntimeError((name, reply))
            return reply

        ask('turbo', enabled=0 if args.mode == 'identity' else 1)
        ask('clear_input')
        ask('input_route_clear')
        record = json.loads(args.route.read_text())
        route = []
        for row in record['pads']:
            if 'pad' in row and (not route or route[-1][1] != row['pad']):
                route.append((row['frame'] - record['start'], row['pad']))
        while route and int(route[0][1], 16) == 0xffff:
            route.pop(0)
        for i, (frame, buttons) in enumerate(route):
            end = route[i+1][0] if i+1 < len(route) else record['end']-record['start']+1
            ask('input_route_append', frames=end-frame, buttons=buttons)
        if args.mode == 'timing':
            ask('window_size', w=960, h=540)
        previous = ask('savestate_status')
        ask('frame_fingerprint', reset_on_load=1)
        ask('savestate', op='load', slot=args.slot)
        deadline = time.monotonic() + 20
        while True:
            status = ask('savestate_status')
            if status != previous and status.get('last_ok') and not status.get('pending', 0):
                break
            if time.monotonic() > deadline:
                raise TimeoutError(('load', status))
            time.sleep(.01)
        fp = ask('frame_fingerprint', count=1)
        deadline = time.monotonic() + 10
        while not fp['entries']:
            if time.monotonic() > deadline:
                raise TimeoutError('first loaded frame')
            time.sleep(.01)
            fp = ask('frame_fingerprint', count=1)
        start_frame = fp['entries'][0]['frame']
        route_start = start_frame + route[0][0]
        if ask('frame')['frame'] >= route_start:
            raise RuntimeError('missed route start; rerun from a fresh process')
        ask('input_route_start', at_frame=route_start)
        result = dict(load=status, start_frame=start_frame, route_start=route_start,
                      route=str(args.route), slot=args.slot,
                      route_sha256=hashlib.sha256(args.route.read_bytes()).hexdigest(),
                      environment={k: os.environ.get(k) for k in
                          ('PSX_INTERNAL_RESOLUTION', 'PSX_SUPERSAMPLE', 'PSX_DYNRES',
                           'PSX_RENDER_PASS_VERIFY', 'PSX_VSYNC', 'PSX_OVERLAY_BACKEND',
                           'PSX_RUNTIME_PERF_DIAG', 'PSX_RUNTIME_PERF_DIAG_MS')})
        if args.mode == 'identity':
            deadline = time.monotonic() + 300
            while True:
                fp = ask('frame_fingerprint', count=args.frames)
                if len(fp['entries']) >= args.frames:
                    break
                if time.monotonic() > deadline:
                    raise TimeoutError('fingerprint window')
                time.sleep(.02)
            result['fingerprint'] = fp
        else:
            deadline = time.monotonic() + 300
            while ask('frame')['frame'] < start_frame + args.warmup_frames:
                if time.monotonic() > deadline:
                    raise TimeoutError('driving warmup')
                time.sleep(.02)
            ask('turbo', enabled=0)
            shot_prefix = args.output.with_suffix('').resolve()
            ask('screenshot', path=str(shot_prefix)+'-before.png')
            result['before'] = {n: ask(n) for n in
                                ('frame', 'render_pass_stats', 'gl_interp', 'render_thread')}
            samples = []
            start = time.monotonic()
            while time.monotonic() - start < args.seconds:
                if args.sample_passes:
                    samples.append(dict(seconds=time.monotonic()-start,
                                        phase=ask('phase_profile', window=1),
                                        frame_perf=cmd({'cmd': 'frame_perf'}, port=args.port),
                                        passes=ask('render_pass_stats')))
                elif args.sample_threads:
                    samples.append(dict(seconds=time.monotonic()-start,
                                        threads=ask('render_thread')))
                time.sleep(.1)
            result.update(seconds=time.monotonic()-start, samples=samples)
            # Freeze the window's counter receipts before screenshot readbacks
            # and post-run profile queries let more game frames execute.
            for name in ('frame', 'render_pass_stats', 'dispatch_stats', 'gl_interp', 'render_thread'):
                result[name] = ask(name)
            result['end_frame'] = result['frame']['frame']
            ask('screenshot', path=str(shot_prefix)+'-after.png')
            result['frame_wall_ring'] = ask('frame_wall_ring', n=20000)
            result['host_span_ring'] = ask('host_span_ring', n=65536)
            # Keep raw sampled module attribution for each 100 ms bucket.
            ring = result['frame_wall_ring']
            rows = list(zip(*(iter(ring['f']),) * ring['stride']))
            groups = {}
            first_frame = result['before']['frame']['frame']
            for row in rows:
                if first_frame <= row[0] <= result['end_frame']:
                    groups.setdefault(row[1] // 100000, []).append(row)
            result['windows'] = []
            for bucket, frames in groups.items():
                result['windows'].append(dict(
                    start_us=bucket * 100000, frames=frames,
                    profile=ask('host_profile', frame_lo=frames[0][0],
                                frame_hi=frames[-1][0], top=400)))
            result['pass_profile'] = ask('host_profile', frame_lo=first_frame,
                                        frame_hi=result['end_frame'], top=400, **{'pass': 1})
        for name in ('render_pass_stats', 'dispatch_stats', 'frame', 'gl_interp'):
            if name not in result:
                result[name] = ask(name)
        result['phase'] = ask('read_ram', addr='0x800FF860', len=4)
        result['ok'] = result['dispatch_stats']['miss_total'] == 0
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    summary = {k: v for k, v in result.items() if k not in
               ('before', 'samples', 'fingerprint', 'render_pass_stats', 'gl_interp',
                'dispatch_stats', 'windows', 'host_span_ring', 'frame_wall_ring',
                'render_thread', 'pass_profile')}
    print(json.dumps(summary, indent=2))
    return 0 if result['ok'] else 1


if __name__ == '__main__':
    raise SystemExit(main())

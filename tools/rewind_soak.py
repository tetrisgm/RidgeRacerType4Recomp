#!/usr/bin/env python3
"""Measure every native rewind capture in a hidden race; preserve all outliers."""
import argparse
import json
import time
from pathlib import Path
from dbg import cmd


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--port', type=int, default=4798)
    ap.add_argument('--seconds', type=float, default=600)
    ap.add_argument('--reload-slot', type=int, help='Reload race every 240 seconds to avoid race timeout')
    ap.add_argument('--out', type=Path, required=True)
    args = ap.parse_args()
    def request(name, **kw):
        result = cmd({'cmd': name, **kw}, port=args.port)
        assert result.get('ok'), result
        return result
    video=request('video_info')
    pgxp=request('pgxp')
    counters_before=request('mod_counters')
    initial = request('rewind_status', hold=0, verify=0, reset_stats=1)
    assert initial['enabled'] and not initial['title_blocked'], initial
    start = time.monotonic()
    deadline = start + args.seconds
    reload_at = start + 240
    samples, reloads = [], []
    while time.monotonic() < deadline:
        time.sleep(min(5, max(0, deadline-time.monotonic())))
        now = time.monotonic()
        if args.reload_slot is not None and now >= reload_at:
            reloads.append({'seconds':now-start, 'reply':request('savestate',op='load',slot=args.reload_slot)})
            reload_at += 240
        sample = request('rewind_status')
        samples.append(sample)
        if len(samples) % 12 == 0:
            print(json.dumps({'seconds':round(now-start), **{k:sample[k] for k in ('captures','p50_ms','p99_ms','max_ms')}}), flush=True)
    end = request('rewind_status')
    dispatch = request('dispatch_stats')
    counters_after=request('mod_counters')
    def effects_count(counters):
        return next((c['count'] for c in counters['counters'] if c['name']=='r4.effects.draw'),0)
    effects_draws=effects_count(counters_after)-effects_count(counters_before)
    mean = end['capture_ms']/end['captures'] if end['captures'] else 0
    frame_delta=end['frame']-initial['frame']
    capture_gap=frame_delta-end['captures']
    passed = (end['captures'] > 1000 and mean < .5 and end['max_ms'] < 2
              and abs(capture_gap)<=2 and effects_draws > 0 and not end['failures'] and not dispatch['miss_total']
              and not dispatch['segment_miss_total'])
    report = dict(result='PASS' if passed else 'FAIL', elapsed_seconds=time.monotonic()-start,
                  mean_ms=mean, frame_delta=frame_delta,capture_gap=capture_gap,initial=initial, end=end, samples=samples,
                  reloads=reloads, dispatch=dispatch,video=video,pgxp=pgxp,effects_draws=effects_draws,
                  counters_before=counters_before,counters_after=counters_after)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k not in ('samples','dispatch','initial')},indent=2))
    return 0 if passed else 1


if __name__ == '__main__':
    raise SystemExit(main())

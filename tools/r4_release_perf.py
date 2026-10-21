#!/usr/bin/env python3
"""Summarize production frame-ring CSV and existing release perf output."""
import argparse
import csv
import json
import re
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('frames', type=Path)
p.add_argument('stdout', type=Path)
p.add_argument('--start', type=int, default=900)
p.add_argument('--end', type=int)
p.add_argument('--clock', choices=('callback', 'completed'), default='callback',
               help='keep callback-arrival timing or measure after frontend pacing/completion')
p.add_argument('--seconds', type=float, help='wall-time window after the start frame')
a = p.parse_args()
if a.seconds is not None and not 0 < a.seconds <= 60:
    p.error('--seconds must be greater than zero and at most 60')
if a.seconds is not None and a.end is not None:
    p.error('--seconds and --end are mutually exclusive')
rows = [{k: int(v) for k, v in row.items()} for row in csv.DictReader(a.frames.open())]
clock = 'us' if a.clock == 'callback' else 'completed_us'
if rows and clock not in rows[0]: p.error('receipt has no requested clock')
if a.seconds is not None:
    rows = [r for r in rows if r['relative'] >= a.start]
    if not rows: p.error('receipt does not contain the requested start frame')
    deadline = rows[0][clock] + a.seconds*1e6
    last = next((i for i,r in enumerate(rows) if r[clock] >= deadline), None)
    if last is None: p.error('receipt does not cover the requested wall-time window')
    rows = rows[:last+1]
    end = rows[-1]['relative']
else:
    end = a.end if a.end is not None else 2700
    rows = [r for r in rows if a.start <= r['relative'] <= end]
if len(rows) != end-a.start+1 or any(b['relative'] != x['relative']+1 for x,b in zip(rows, rows[1:])):
    p.error('frame receipt does not cover the full requested interval')
if any(not r[clock] for r in rows): p.error('requested clock has incomplete frame records')
if a.clock == 'completed' and any(not x['us'] <= x[clock] <= b['us'] for x,b in zip(rows, rows[1:])):
    p.error('completion stamp is outside its callback interval')
gaps = [(b[clock]-x[clock])/1000 for x,b in zip(rows, rows[1:])]
if any(x <= 0 for x in gaps): p.error('frame clock is not strictly increasing')
pattern = re.compile(r'frames=(\d+) elapsed_ms=(\d+) fps=([\d.]+).*emu_busy=([\d.]+)%')
windows = []
for line in a.stdout.read_text().splitlines():
    m = pattern.search(line)
    if m and rows[0]['frame'] <= int(m[1]) <= rows[-1]['frame']:
        windows.append(dict(frame=int(m[1]), elapsed_ms=int(m[2]), fps=float(m[3]), busy=float(m[4])))
if not windows:
    p.error('no release frame reports cover the requested interval')
duration = (rows[-1][clock]-rows[0][clock])/1e6
out = dict(clock=a.clock, frames=len(gaps), seconds=duration, fps=len(gaps)/duration,
           gap_max_ms=max(gaps), gap_p95_ms=sorted(gaps)[int(len(gaps)*.95)],
           gaps_over25=sum(x > 25 for x in gaps),
           gaps_over50=sum(x > 50 for x in gaps), windows=len(windows),
           busy90=sum(x['busy'] >= 90 for x in windows),
           busy_p95=sorted(x['busy'] for x in windows)[int(len(windows)*.95)],
           busy_p99=sorted(x['busy'] for x in windows)[int(len(windows)*.99)],
           worst=sorted(windows, key=lambda x:-x['busy'])[:5])
if 'guest_us' in rows[0]:
    # Work/process counters are sampled at callback entry, independently of
    # the requested gap clock; do not imply that they share its boundaries.
    out['work_counter_clock'] = 'callback'
    out['largest_gaps'] = sorted([dict(relative=b['relative'], gap_ms=gap,
        callback_interval_ms=(b['us']-x['us'])/1000,
        guest_ms=(b['guest_us']-x['guest_us'])/1000,
        pacer_ms=(b['pacer_us']-x['pacer_us'])/1000,
        queue_ms=(b['queue_us']-x['queue_us'])/1000,
        faults=b['faults']-x['faults'], io_kb=b['io_kb']-x['io_kb'])
        for x,b,gap in zip(rows, rows[1:], gaps) if gap > 25],
        key=lambda x:-x['gap_ms'])[:10]
present_path = Path(str(a.frames)+'.present.csv')
if present_path.exists():
    swaps = [int(r['us']) for r in csv.DictReader(present_path.open())]
    if any(b <= x for x,b in zip(swaps,swaps[1:])):
        p.error('presentation clock is not strictly increasing')
    start_us, end_us = rows[0][clock], rows[-1][clock]
    before = [i for i,t in enumerate(swaps) if t <= start_us]
    after = next((i for i,t in enumerate(swaps) if t >= end_us), None)
    if before and after is not None:
        selected = swaps[before[-1]:after+1]
        pgaps = [(b-a)/1000 for a,b in zip(selected,selected[1:])]
        out['presentation'] = dict(
            covered=True, presents=sum(start_us <= t < end_us for t in swaps),
            fps=sum(start_us <= t < end_us for t in swaps)/duration,
            gap_max_ms=max(pgaps), gaps_over25=sum(x>25 for x in pgaps),
            gaps_over50=sum(x>50 for x in pgaps))
    else:
        out['presentation'] = dict(covered=False)
print(json.dumps(out, indent=2))

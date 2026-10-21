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
p.add_argument('--seconds', type=float, help='wall-time window after the start frame')
a = p.parse_args()
if a.seconds is not None and not 0 < a.seconds <= 60:
    p.error('--seconds must be greater than zero and at most 60')
if a.seconds is not None and a.end is not None:
    p.error('--seconds and --end are mutually exclusive')
rows = [{k: int(v) for k, v in row.items()} for row in csv.DictReader(a.frames.open())]
if a.seconds is not None:
    rows = [r for r in rows if r['relative'] >= a.start]
    if not rows: p.error('receipt does not contain the requested start frame')
    deadline = rows[0]['us'] + a.seconds*1e6
    last = next((i for i,r in enumerate(rows) if r['us'] >= deadline), None)
    if last is None: p.error('receipt does not cover the requested wall-time window')
    rows = rows[:last+1]
    end = rows[-1]['relative']
else:
    end = a.end if a.end is not None else 2700
    rows = [r for r in rows if a.start <= r['relative'] <= end]
if len(rows) != end-a.start+1 or any(b['relative'] != x['relative']+1 for x,b in zip(rows, rows[1:])):
    p.error('frame receipt does not cover the full requested interval')
gaps = [(b['us']-x['us'])/1000 for x,b in zip(rows, rows[1:])]
pattern = re.compile(r'frames=(\d+) elapsed_ms=(\d+) fps=([\d.]+).*emu_busy=([\d.]+)%')
windows = []
for line in a.stdout.read_text().splitlines():
    m = pattern.search(line)
    if m and rows[0]['frame'] <= int(m[1]) <= rows[-1]['frame']:
        windows.append(dict(frame=int(m[1]), elapsed_ms=int(m[2]), fps=float(m[3]), busy=float(m[4])))
if not windows:
    p.error('no release frame reports cover the requested interval')
duration = (rows[-1]['us']-rows[0]['us'])/1e6
out = dict(frames=len(gaps), seconds=duration, fps=len(gaps)/duration,
           gap_max_ms=max(gaps), gap_p95_ms=sorted(gaps)[int(len(gaps)*.95)],
           gaps_over25=sum(x > 25 for x in gaps),
           gaps_over50=sum(x > 50 for x in gaps), windows=len(windows),
           busy90=sum(x['busy'] >= 90 for x in windows),
           busy_p95=sorted(x['busy'] for x in windows)[int(len(windows)*.95)],
           busy_p99=sorted(x['busy'] for x in windows)[int(len(windows)*.99)],
           worst=sorted(windows, key=lambda x:-x['busy'])[:5])
if 'guest_us' in rows[0]:
    out['largest_gaps'] = sorted([dict(relative=b['relative'], gap_ms=gap,
        guest_ms=(b['guest_us']-x['guest_us'])/1000,
        pacer_ms=(b['pacer_us']-x['pacer_us'])/1000,
        queue_ms=(b['queue_us']-x['queue_us'])/1000,
        faults=b['faults']-x['faults'], io_kb=b['io_kb']-x['io_kb'])
        for x,b,gap in zip(rows, rows[1:], gaps) if gap > 25],
        key=lambda x:-x['gap_ms'])[:10]
print(json.dumps(out, indent=2))

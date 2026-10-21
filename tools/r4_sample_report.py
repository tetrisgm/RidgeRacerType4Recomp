#!/usr/bin/env python3
"""Resolve external Windows instruction samples against a Release executable."""
import argparse
import bisect
from collections import Counter
import csv
import json
import struct
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'psxrecomp/tools'))
from host_profile import symbols, find_nm

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('samples', type=Path)
p.add_argument('exe', type=Path)
p.add_argument('--start', type=float, default=15)
p.add_argument('--end', type=float, default=75)
p.add_argument('--nm')
p.add_argument('--tid', type=int, help='restrict attribution to one captured thread')
a = p.parse_args()
table = symbols(find_nm(a.nm), str(a.exe))
addresses = [x[0] for x in table]
with a.exe.open('rb') as f:
    header = f.read(4096)
    pe = struct.unpack_from('<I', header, 0x3c)[0]
    sections = struct.unpack_from('<H', header, pe+6)[0]
    optional = struct.unpack_from('<H', header, pe+20)[0]
    f.seek(pe+24+optional)
    code_ranges = []
    for _ in range(sections):
        section = f.read(40)
        size, start = struct.unpack_from('<II', section, 8)
        if struct.unpack_from('<I', section, 36)[0] & 0x20000000:
            code_ranges.append((start, start+size))
def is_code(rva):
    return any(lo <= rva < hi for lo,hi in code_ranges)
def name(rva):
    i = bisect.bisect_right(addresses, rva)-1
    return table[i][1] if i >= 0 else hex(rva)
pc = Counter()
returns = Counter()
total = 0
unwound = False
cpu_ranges = {}
for row in csv.DictReader(a.samples.open()):
    if not a.start*1e6 <= int(row['us']) <= a.end*1e6:
        continue
    if a.tid is not None and int(row['tid']) != a.tid:
        continue
    total += 1
    unwound |= row.get('stack_kind') == 'unwind'
    if row.get('cpu_us'):
        point = (int(row['us']), int(row['cpu_us']))
        span = cpu_ranges.setdefault(row['tid'], [point, point])
        span[1] = point
    if row['module'].lower() == a.exe.name.lower():
        pc[name(int(row['rva'], 16))] += 1
    else:
        pc[row['module']] += 1
        # Only executable frames are retained. Old captures contain scan
        # candidates; stack mode contains DbgHelp-unwound frames. Counts here
        # describe samples whose PC was outside the executable, not complete
        # inclusive CPU time. Deduplicate each sample's frame names.
        returns.update({name(int(row[f'stack{i}'], 16)) for i in range(16)
                        if is_code(int(row[f'stack{i}'], 16))})
cpu_percent = {tid: round(100*(end[1]-start[1])/(end[0]-start[0]), 2)
               for tid,(start,end) in cpu_ranges.items() if end[0] > start[0]}
print(json.dumps(dict(samples=total, seconds=[a.start, a.end],
                     thread_cpu_percent=cpu_percent,
                     pc=pc.most_common(30),
                     **{('unwound_frames' if unwound else 'blocked_return_candidates'):
                        returns.most_common(30)}), indent=2))

#!/usr/bin/env python3
"""Convert recorded applied VBlank pads to the runtime's release replay file."""
import argparse
import hashlib
import json
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('record', type=Path)
p.add_argument('output', type=Path)
a = p.parse_args()
if a.output.exists():
    p.error('output exists; choose a new path')
data = json.loads(a.record.read_text())
rows = [row for row in data['pads'] if 'pad' in row]
steps = []
for i, row in enumerate(rows):
    if row['frame'] != data['start'] + i:
        p.error('record must contain one applied pad word per consecutive VBlank')
    buttons = int(row['pad'], 16)
    if steps and steps[-1][1] == buttons:
        steps[-1][0] += 1
    else:
        steps.append([1, buttons])
text = '# applied digital pad route; frames hexadecimal-buttons\n'
text += ''.join(f'{frames} {buttons:04x}\n' for frames, buttons in steps)
a.output.write_text(text, encoding='ascii')
print(json.dumps(dict(frames=len(rows), steps=len(steps),
                      source_sha256=hashlib.sha256(a.record.read_bytes()).hexdigest()), indent=2))

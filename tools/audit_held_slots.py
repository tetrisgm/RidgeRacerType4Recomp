"""Attribute every held slot in an integrated audit to recorded render state.

Uses the same derived 60 Hz grid as the lap harness, not physical scanout.
Intervals are evidence for ownership at the slot, not proof of CPU execution:
driver waits and OS descheduling are included in render elapsed time.
"""
import bisect
import collections
import csv
import json
import sys
from pathlib import Path


def analyze(folder):
    folder = Path(folder)
    raw = json.loads((folder / 'raw.json').read_text())
    start, end = raw['start'], raw['end']
    presents = [r for r in csv.DictReader((folder / 'present.csv').open())
                if start <= int(r['time_ns']) <= end]
    events = [{k: int(v) for k, v in r.items()}
              for r in csv.DictReader((folder / 'slots.csv').open())]
    states = sorted((r for r in events if r['event'] == 0), key=lambda r: r['begin_ns'])
    times = [r['begin_ns'] for r in states]
    work = sorted((r for r in events if r['event'] in (1, 2)), key=lambda r: r['begin_ns'])
    work_times = [r['begin_ns'] for r in work]
    counts, details = collections.Counter(), []
    i, last_hash = 0, None
    for slot in range(int((end - start) * 60 / 1e9)):
        tick = start + slot * 1000000000 // 60
        while i + 1 < len(presents) and int(presents[i + 1]['time_ns']) <= tick:
            i += 1
        if not presents or int(presents[i]['time_ns']) > tick:
            continue
        h = presents[i]['image_hash']
        held = h == last_hash
        last_hash = h
        if not held:
            continue
        si = bisect.bisect_right(times, tick) - 1
        wi = bisect.bisect_right(work_times, tick) - 1
        state = states[si] if si >= 0 else {}
        active = work[wi] if wi >= 0 and work[wi]['end_ns'] >= tick else None
        if active:
            reason = 'render_late'
            operation = 'prepare' if active['event'] == 1 else 'draw'
        elif state.get('snapped'):
            reason, operation = 'interpolation_failed', ''
        elif not state.get('period_ns'):
            reason, operation = 'no_next_endpoint_ready', ''
        else:
            reason, operation = 'presenter_deadline_missed', ''
        counts[reason] += 1
        details.append(dict(slot=slot, time_ns=tick, reason=reason, operation=operation,
                            sequence=state.get('sequence', 0),
                            due_ns=state.get('due_ns', 0),
                            last_present_ns=int(presents[i]['time_ns'])))
    with (folder / 'held-slots.csv').open('w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=['slot', 'time_ns', 'reason', 'operation',
                                             'sequence', 'due_ns', 'last_present_ns'])
        writer.writeheader()
        writer.writerows(details)
    result = dict(held=len(details), reasons=dict(counts),
                  render_operations=dict(collections.Counter(r['operation'] for r in details
                                                              if r['operation'])))
    (folder / 'held-reasons.json').write_text(json.dumps(result, indent=2))
    print(json.dumps(result))
    return result


if __name__ == '__main__':
    analyze(sys.argv[1])

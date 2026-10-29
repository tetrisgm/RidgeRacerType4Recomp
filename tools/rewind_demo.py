#!/usr/bin/env python3
"""Verify live hold-to-rewind via TCP; run with dummy audio and a hidden window.

python tools/rewind_demo.py --port 4798 --out build/rewind-proof.json
"""
import argparse
import json
import time
from pathlib import Path
from dbg import cmd


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--port', type=int, default=4798)
    ap.add_argument('--seconds', type=float, default=10)
    ap.add_argument('--out', type=Path, required=True)
    a = ap.parse_args()
    def status(**kw):
        return cmd({'cmd': 'rewind_status', **kw}, port=a.port)
    before = status(hold=0, verify=0)
    assert before['enabled'], before
    time.sleep(a.seconds)
    captured = status()
    count = captured['captures'] - before['captures']
    assert count > 30, captured
    mean = (captured['capture_ms'] - before['capture_ms']) / count
    samples = []
    status(verify=1, hold=1)
    try:
        for _ in range(40):
            time.sleep(.05)
            samples.append(status())
    finally:
        released = status(hold=0, verify=0)
    time.sleep(.5)
    resumed = status()
    report = dict(capture_frames=count, mean_capture_ms=mean,
                  before=before, captured=captured, reverse_samples=samples,
                  released=released, resumed=resumed)
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text(json.dumps(report, indent=2) + '\n')
    active = [s for s in samples if s['open']]
    assert len(active) >= 30, samples
    assert all(y['frame'] <= x['frame'] for x, y in zip(active, active[1:]))
    restores = released['restores'] - captured['restores']
    assert restores >= 30, report
    assert released['verified'] - captured['verified'] == restores, report
    assert released['verify_failures'] == captured['verify_failures'], report
    assert resumed['captures'] > released['captures'] and not resumed['open'], report
    assert resumed['failures'] == 0, report
    assert mean < .5, report
    print(json.dumps(dict(result='PASS', capture_frames=count,
                          mean_capture_ms=mean, exact_restores=restores,
                          proof=str(a.out)), indent=2))


if __name__ == '__main__':
    main()

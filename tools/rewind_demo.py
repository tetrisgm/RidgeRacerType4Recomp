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
    ap.add_argument('--host', action='store_true', help='Use mapped host Y instead of debug hold')
    ap.add_argument('--visual', type=Path, help='Save enhanced forward/held/released present captures')
    ap.add_argument('--hold-seconds', type=float, default=2)
    a = ap.parse_args()
    def status(**kw):
        return cmd({'cmd': 'rewind_status', **kw}, port=a.port)
    shots=[]
    def shot(name):
        if not a.visual: return
        a.visual.mkdir(parents=True,exist_ok=True)
        path=(a.visual/(name+'.png')).resolve()
        prior=cmd({'cmd':'present_shot_seq'},port=a.port)['seq']
        reply=cmd({'cmd':'present_shot','path':str(path)},port=a.port)
        assert reply.get('ok'),reply
        end=time.monotonic()+5
        while time.monotonic()<end:
            done=cmd({'cmd':'present_shot_seq'},port=a.port)
            if done['seq']>prior:
                assert done['wrote'],done
                shots.append(dict(path=str(path),state=status(),completion=done));return
            time.sleep(.02)
        raise AssertionError('present capture did not complete')
    before = status(hold=0, verify=0)
    assert before['enabled'], before
    time.sleep(a.seconds)
    captured = status()
    count = captured['captures'] - before['captures']
    assert count > 30, captured
    mean = (captured['capture_ms'] - before['capture_ms']) / count
    shot("forward")
    samples = []
    status(verify=1, hold=0 if a.host else 1)
    if a.host:
        cmd({'cmd':'set_input','layer':'host','buttons':'0xEFFF'},port=a.port)
    menus=[]
    try:
        for i in range(max(40,round(a.hold_seconds/.05))):
            time.sleep(.05)
            samples.append(status())
            if i in (8,24): shot("held-"+str(i))
            menus.append(cmd({'cmd':'runtime_menu'},port=a.port))
    finally:
        if a.host:
            cmd({'cmd':'set_input','layer':'host','buttons':'0xFFFF'},port=a.port)
        released = status(hold=0, verify=0)
    time.sleep(.5)
    resumed = status()
    shot("released")
    report = dict(capture_frames=count, mean_capture_ms=mean,
                  before=before, captured=captured, reverse_samples=samples,
                  released=released, resumed=resumed,host_input=a.host,menus=menus,shots=shots,
                  video=cmd({"cmd":"video_info"},port=a.port),
                  dispatch=cmd({"cmd":"dispatch_stats"},port=a.port))
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text(json.dumps(report, indent=2) + '\n')
    assert all(m.get('ok') and not m['open'] for m in menus), menus
    active = [s for s in samples if s['open']]
    assert len(active) >= 30, samples
    assert all(y['frame'] <= x['frame'] for x, y in zip(active, active[1:]))
    restores = released['restores'] - captured['restores']
    assert restores >= 30, report
    assert released['verified'] - captured['verified'] == restores, report
    assert released['verify_failures'] == captured['verify_failures'], report
    assert resumed['captures'] > released['captures'] and not resumed['open'], report
    assert resumed['failures'] == 0, report
    assert released['packet_replays']-captured['packet_replays']==restores, report
    assert released['packet_misses']==captured['packet_misses'], report
    assert all(s['packet_bytes']<=512*1024*1024 for s in samples),report
    assert report['dispatch']['miss_total']==report['dispatch']['segment_miss_total']==0,report
    assert mean < .5, report
    print(json.dumps(dict(result='PASS', capture_frames=count,
                          mean_capture_ms=mean, exact_restores=restores, enhanced_replays=released['packet_replays']-captured['packet_replays'],
                          proof=str(a.out)), indent=2))


if __name__ == '__main__':
    main()

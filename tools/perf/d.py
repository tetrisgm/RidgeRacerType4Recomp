import os, sys, json, time
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
from dbg import cmd
P = int(os.environ.get('R4_DEBUG_PORT', '4797'))
def c(**k):
    return cmd(k, port=P)
def shot(path, small=None):
    prior = c(cmd='present_shot_seq')['seq']
    r = c(cmd='present_shot', path=path)
    assert r.get('ok'), r
    end = time.monotonic() + 8
    while time.monotonic() < end:
        d = c(cmd='present_shot_seq')
        if d['seq'] > prior:
            if small:
                from PIL import Image
                im = Image.open(path); im.thumbnail((1000, 600)); im.save(small)
            return d
        time.sleep(.02)
    raise SystemExit('shot timeout')
if __name__ == '__main__':
    a = sys.argv[1:]
    if a[0] == 'raw':
        print(json.dumps(c(**json.loads(a[1])))[:3000])
    elif a[0] == 'shot':
        print(shot(a[1], a[2] if len(a) > 2 else None))
    elif a[0] == 'burst':  # burst OUTDIR N interval
        n = int(a[2]); iv = float(a[3])
        for i in range(n):
            shot(f'{a[1]}/b{i:02d}.png', f'{a[1]}/s{i:02d}.png'); time.sleep(iv)

"""Per-op render-thread cost over 10 s of autopilot driving (needs PSX_RENDER_OP_PROFILE=1)."""
import os, subprocess, sys, time
from d import c
PY = sys.executable; HERE = os.path.dirname(os.path.abspath(__file__))
def snap():
    r = c(cmd="render_thread"); return {op: (n, ns) for op, n, ns in r.get("ops", [])}, r["frames_consumed"]
c(cmd="savestate", op="load", slot=0); time.sleep(0.5)
ap = subprocess.Popen([PY, os.path.join(HERE, "autopilot.py"), "14"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(2); a, ca = snap(); time.sleep(10); b, cb = snap(); ap.wait()
pk = max(1, cb - ca)
rows = []
for op in b:
    n = b[op][0] - a.get(op, (0, 0))[0]; ns = b[op][1] - a.get(op, (0, 0))[1]
    if n: rows.append((ns, op, n))
rows.sort(reverse=True)
print("packets", pk, "total ms/pkt", sum(r[0] for r in rows) / 1e6 / pk)
for ns, op, n in rows[:18]:
    print("op %3d  count/pkt %7.1f  ms/pkt %6.2f  us/op %6.1f" % (op, n / pk, ns / 1e6 / pk, ns / 1e3 / n))

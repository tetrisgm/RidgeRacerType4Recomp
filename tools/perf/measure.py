"""Load slot 0, drive 14 s with the autopilot, report render-thread cost per packet.
Usage: python measure.py [label] [RATE]   (RATE sets packet_interpolation rate first)"""
import json, os, subprocess, sys, time
from d import c

label = sys.argv[1] if len(sys.argv) > 1 else "run"
if len(sys.argv) > 2:
    c(cmd="packet_interpolation", rate=int(sys.argv[2]))
PY = sys.executable
HERE = os.path.dirname(os.path.abspath(__file__))

def snap():
    r = c(cmd="render_thread"); p = r["packet_cpu"]
    d = dict(prod=r["frames_produced"], cons=r["frames_consumed"], rep=r["frames_replaced"],
             busy=r["render_busy_ms"], draw_ns=p["draw_ns"], draws=p["draws"], prep_ns=p["prepare_ns"],
             ntb=r["native_texture_bytes"], ntu=r["native_texture_uploads"], holds=p["gap_holds"])
    for i, v in enumerate(p.get("phase_ns", [])): d["ph%d" % i] = v
    for i, v in enumerate(p.get("tri_ns", [])): d["tri%d" % i] = v
    for i, v in enumerate(p.get("prep_ns", [])): d["prep%d" % i] = v
    g = r.get("gpu_packet", {})
    d["gpu_samples"] = g.get("samples", 0); d["gpu_ns"] = g.get("total_ns", 0); d["gpu_present_ns"] = g.get("present_ns", 0)
    pt = r.get("present_thread", {})
    d["swaps"] = pt.get("presents", 0); d["gap25"] = pt.get("gaps_over_25ms", 0)
    return d

c(cmd="savestate", op="load", slot=0); time.sleep(0.5)
ap = subprocess.Popen([PY, os.path.join(HERE, "autopilot.py"), "14"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(2); a = snap(); time.sleep(10); b = snap(); ap.wait()
d = {k: b[k] - a[k] for k in a}
cons = max(1, d["cons"]); draws = max(1, d["draws"])
line = ("%s: produced %d consumed %d replaced %d (%.0f%%) busy_ms %d | per draw ms: total %.1f" %
        (label, d["prod"], d["cons"], d["rep"], 100.0 * d["rep"] / max(1, d["prod"]), d["busy"], d["draw_ns"] / 1e6 / draws))
names = ["seed", "restage", "replay", "present", "flush"]
ph = [d.get("ph%d" % i, 0) / 1e6 / draws for i in range(5)]
line += " [" + " ".join("%s %.1f" % (n, v) for n, v in zip(names, ph)) + "]"
line += " | prepare ms/pkt %.1f | upload KB/pkt %d (%d uploads) | gap_holds %d | swaps %d gaps>25ms %d" % (
    d["prep_ns"] / 1e6 / cons, d["ntb"] / 1024 / cons, d["ntu"], d["holds"], d["swaps"], d["gap25"])
print(line)
if "tri0" in d:
    print("  tri ms/draw: hd %.2f packflush %.2f append %.2f batchdraw %.2f | prep ms/pkt: append %.2f history %.2f pair %.2f rest %.2f" % tuple(
        [d["tri%d" % i] / 1e6 / draws for i in range(4)] + [d["prep%d" % i] / 1e6 / cons for i in range(4)]))
if d["gpu_samples"]:
    print("  GPU ms/packet: total %.2f (present %.2f) over %d samples" % (d["gpu_ns"] / 1e6 / d["gpu_samples"], d["gpu_present_ns"] / 1e6 / d["gpu_samples"], d["gpu_samples"]))
pr = c(cmd="packet_interpolation")["presenter"]
print("presenter: rate %d interval_ms %.1f late %d resyncs %d in_between %d endpoints %d" % (
    pr["rate"], pr["interval_ms"], pr["late_packets"], pr["resyncs"], pr["in_between"], pr["endpoints"]))

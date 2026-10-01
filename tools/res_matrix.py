#!/usr/bin/env python3
"""res_matrix.py - check every Internal resolution preset in one scene.

For each preset: launch the build with PSX_INTERNAL_RESOLUTION set, load a
savestate slot (turbo to the load, then real time), optionally hold buttons,
and record video_info (requested vs effective scale, internal lines, GL limit,
high-resolution window, drawable), screenshot_hires (the internal-resolution
frame), present_shot (what the window shows) and frame_perf. Writes one
directory per preset plus summary.json, and prints one line per preset.

Usage:
  python3 tools/res_matrix.py OUT_DIR --slot 6 [--press cross]
      [--presets native,720p,1080p,1440p,4k,5k,8k,display] [--port 4797]
      [--build build] [--no-hires]

Make the savestate first: reach the scene (tools/smoke.py), then
  python3 tools/dbg.py savestate op=save slot=6
The build needs -DPSX_DEBUG_TOOLS=ON and a window (not --headless) for
present_shot. Each run opens and closes the game window.
"""
import argparse
import json
import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dbg import cmd  # noqa: E402
from pad import mask  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def wait_up(port, secs=90):
    t = time.time() + secs
    while time.time() < t:
        try:
            return cmd({"cmd": "ping"}, port=port, timeout=2)
        except OSError:
            time.sleep(0.3)
    raise SystemExit("debug server not reachable on port %d" % port)


def wait_frames(n, port):
    target = cmd({"cmd": "frame"}, port=port).get("frame", 0) + n
    while cmd({"cmd": "frame"}, port=port).get("frame", 0) < target:
        time.sleep(0.02)


def present_shot(path, port, timeout=20):
    seq0 = cmd({"cmd": "present_shot_seq"}, port=port).get("seq", 0)
    cmd({"cmd": "present_shot", "path": path}, port=port)
    t = time.time() + timeout
    while time.time() < t:
        r = cmd({"cmd": "present_shot_seq"}, port=port)
        if r.get("seq", 0) != seq0:
            return r
        time.sleep(0.05)
    return {"timeout": True}


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out")
    ap.add_argument("--slot", type=int, required=True)
    ap.add_argument("--press", default="", help="comma list of buttons to hold after the load")
    ap.add_argument("--presets", default="native,720p,1080p,1440p,4k,5k,8k,display")
    ap.add_argument("--port", type=int, default=4797)
    ap.add_argument("--build", default="build")
    ap.add_argument("--settle", type=float, default=6.0, help="real seconds before measuring")
    ap.add_argument("--no-hires", action="store_true")
    a = ap.parse_args()
    out = os.path.abspath(a.out)
    os.makedirs(out, exist_ok=True)
    rows = []
    for preset in [p for p in a.presets.split(",") if p]:
        od = os.path.join(out, preset)
        os.makedirs(od, exist_ok=True)
        env = dict(os.environ, PSX_INTERNAL_RESOLUTION=preset)
        log = open(os.path.join(od, "runtime.log"), "w")
        proc = subprocess.Popen([os.path.join(ROOT, "tools", "run_r4.sh"), a.build,
                                 "--debug-port", str(a.port)],
                                cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT)
        row = {"preset": preset}
        try:
            wait_up(a.port)
            cmd({"cmd": "turbo", "enabled": 1}, port=a.port)
            wait_frames(100, a.port)
            cmd({"cmd": "savestate", "op": "load", "slot": a.slot}, port=a.port)
            t = time.time() + 20
            while time.time() < t:
                if cmd({"cmd": "savestate_status"}, port=a.port).get("last_op") == "load":
                    break
                time.sleep(0.05)
            if a.press:
                cmd({"cmd": "set_input", "buttons": "0x%04X" % mask(a.press.split(","))},
                    port=a.port)
            cmd({"cmd": "turbo", "enabled": 0}, port=a.port)
            time.sleep(a.settle)
            perf = cmd({"cmd": "frame_perf"}, port=a.port)
            vi = cmd({"cmd": "video_info"}, port=a.port)
            hs = {} if a.no_hires else cmd({"cmd": "screenshot_hires",
                                           "path": os.path.join(od, "hires.png")},
                                          port=a.port, timeout=180)
            ps = present_shot(os.path.join(od, "present.png"), a.port)
            ds = cmd({"cmd": "dispatch_stats"}, port=a.port)
            json.dump({"video_info": vi, "frame_perf": perf, "screenshot_hires": hs},
                      open(os.path.join(od, "info.json"), "w"), indent=1)
            allp = perf.get("all", {})
            row.update({
                "requested": vi.get("requested_scale"), "effective": vi.get("effective_scale"),
                "internal_lines": vi.get("internal_lines"), "windowed": vi.get("windowed"),
                "hires": "%sx%s" % (hs.get("width"), hs.get("height")),
                "drawable": "%sx%s" % (vi.get("drawable_w"), vi.get("drawable_h")),
                "frame_ms": allp.get("total_ms_avg"), "present_gpu_ms": allp.get("present_gpu_ms_avg"),
                "present_shot": ps.get("wrote"), "dispatch_miss": ds.get("miss_total"),
                "segment_miss": ds.get("segment_miss_total"),
            })
        except Exception as exc:  # keep going: one bad preset should not hide the rest
            row["error"] = repr(exc)
        finally:
            try:
                cmd({"cmd": "clear_input"}, port=a.port, timeout=2)
            except OSError:
                pass
            proc.terminate()
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proc.kill()
        print(json.dumps(row), flush=True)
        rows.append(row)
    json.dump(rows, open(os.path.join(out, "summary.json"), "w"), indent=1)
    return 0 if all("error" not in r for r in rows) else 1


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Audit a running, paused/static PGXP scene through the debug server.

Run the game with --hidden-window and PGXP/Car Effects enabled. Pause the
race before invoking this tool. It captures composed presents and compares
their bytes and the ordered guest PGXP triangle stream. A moving scene is
expected to fail; the caller chooses the static interval.
"""
import argparse
import hashlib
import json
import pathlib
import time

from dbg import cmd


def request(command, port):
    result = cmd(command, port=port, timeout=30)
    if not result.get("ok"):
        raise RuntimeError(f"{command['cmd']}: {result}")
    return result


def capture(index, directory, port, expected_burst=1):
    before = request({"cmd": "present_shot_seq"}, port)["seq"]
    path = directory / f"present-{index}.png"
    request({"cmd": "present_shot", "path": str(path)}, port)
    deadline = time.monotonic() + 30
    while time.monotonic() < deadline:
        status = request({"cmd": "present_shot_seq"}, port)
        if status["seq"] > before:
            paths = [path] if path.is_file() else sorted(directory.glob(path.name + "_*.png"))
            if not status.get("wrote") or len(paths) != expected_burst:
                raise RuntimeError(f"present capture failed: {status}")
            break
        time.sleep(0.05)
    else:
        raise TimeoutError("present capture did not complete")
    # A paused title can redraw only every second/third VBlank. An empty ring
    # is not evidence of matching vertex streams: select an actual drawn frame.
    frame = request({"cmd": "frame"}, port)["frame"] - 1
    for candidate in range(frame, max(-1, frame - 20), -1):
        audit = request({"cmd": "pgxp_frame", "frame": candidate}, port)
        if audit["lookups"]:
            frame = candidate
            break
    else:
        raise RuntimeError("no drawn PGXP frame in the last 20 VBlanks")
    native_path = directory / f"native-{index}.png"
    request({"cmd": "screenshot_file", "path": str(native_path)}, port)
    tris = request({"cmd": "pgxp_tri_ring", "frame": frame,
                    "pass": 0, "count": 16384}, port)["tris"]
    # R4 alternates VRAM pages 240 rows apart. Compare display-local PGXP
    # positions and packet content; the backing packet address also alternates.
    page_y16 = 0
    if tris:
        v = tris[0]["v"][0]
        page_y16 = round((v[5] / 65536 - v[3]) / 240) * 240 * 65536
    stream = [{"op": t["op"], "v": [v[:5] + [v[5] - page_y16] + v[6:]
                                         for v in t["v"]]} for t in tris]
    errors = [(v[4] / 65536 - v[2], v[5] / 65536 - v[3], v)
              for t in stream for v in t["v"] if v[6]]
    accuracy = {"max_abs_x": max((abs(x) for x,y,v in errors), default=0),
                "max_abs_y": max((abs(y) for x,y,v in errors), default=0),
                "over_one": sum(max(abs(x),abs(y)) > 1 for x,y,v in errors),
                "onscreen_over_one": sum(max(abs(x),abs(y)) > 1 and
                    0 <= v[2] < 320 and 0 <= v[3] < 240 for x,y,v in errors)}
    kinds = {}
    for t in stream:
        key = str(t["op"])
        kinds[key] = kinds.get(key, 0) + 1
    return {"frame": frame, "image_sha256": hashlib.sha256(paths[0].read_bytes()).hexdigest(),
            "presents": [{"file": p.name, "sha256": hashlib.sha256(p.read_bytes()).hexdigest()} for p in paths],
            "native_sha256": hashlib.sha256(native_path.read_bytes()).hexdigest(),
            "triangles": len(stream), "stream_sha256": hashlib.sha256(
                json.dumps(stream, sort_keys=True, separators=(",", ":")).encode()
            ).hexdigest(), "pgxp": audit, "coordinate_errors": accuracy, "opcodes": kinds, "raster": request({"cmd": "pgxp_raster"}, port)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("out_dir", type=pathlib.Path)
    parser.add_argument("--port", type=int, default=4798)
    parser.add_argument("--samples", type=int, default=3)
    parser.add_argument("--expected-burst", type=int, default=1,
                        help="start runtime with PSX_PRESENT_SHOT_BURST=N to capture consecutive presents")
    parser.add_argument("--replay-slot", type=int)
    parser.add_argument("--replays", type=int, default=2)
    parser.add_argument("--settle-seconds", type=float, default=6,
                        help="allow transient savestate notification to disappear")
    parser.add_argument("--require-effects", action="store_true")
    parser.add_argument("--particle-car", type=int, help="trigger an R4 backfire after each load to audit paused particle stability")
    args = parser.parse_args()
    if args.samples < 2 or args.expected_burst < 1 or args.replays < 1:
        parser.error("need >=2 samples, >=1 burst and >=1 replay")
    if args.particle_car is not None and not 0 <= args.particle_car < 8:
        parser.error("particle car must be in 0..7")
    args.out_dir.mkdir(parents=True, exist_ok=True)
    pgxp = request({"cmd": "pgxp"}, args.port)
    if not pgxp["enabled"] or not pgxp["active"]:
        raise RuntimeError("PGXP must remain active")
    pipeline = request({"cmd": "pgxp_raster"}, args.port)
    if not pipeline.get("pipeline_ready") or pipeline.get("textured_geometry_vertices") != 3:
        raise RuntimeError(f"PGXP GL interpolation pipeline unavailable: {pipeline}")
    effects_before = request({"cmd": "mod_counters"}, args.port)
    samples = []
    particle_baselines = []
    for replay in range(args.replays if args.replay_slot is not None else 1):
        if args.replay_slot is not None:
            request({"cmd": "savestate", "op": "load", "slot": args.replay_slot}, args.port)
            time.sleep(args.settle_seconds)
            state = request({"cmd": "savestate_status"}, args.port)
            if state["pending"] or not state["last_ok"] or state["last_op"] != "load":
                raise RuntimeError(f"savestate load failed: {state}")
        if args.particle_car is not None:
            particle_baselines.append(capture(f"baseline-r{replay}", args.out_dir, args.port, args.expected_burst))
            request({"cmd": "mod_cmd", "name": "r4.effects",
                     "args": f"flame {args.particle_car} 1"}, args.port)
        for i in range(args.samples):
            samples.append(capture(f"r{replay}-{i}", args.out_dir, args.port, args.expected_burst))
    effects_after = request({"cmd": "mod_counters"}, args.port)
    def effects_count(snapshot, name="r4.effects.draw"):
        return sum(c["count"] for c in snapshot["counters"] if c["name"] == name)
    effects_draws = effects_count(effects_after) - effects_count(effects_before)
    simulations = effects_count(effects_after, "r4.effects.simulation") - effects_count(effects_before, "r4.effects.simulation")
    backfires = effects_count(effects_after, "r4.effects.backfire") - effects_count(effects_before, "r4.effects.backfire")
    drawn = [s for s in samples if s["triangles"]]
    report = {"samples": samples, "pgxp_settings": pgxp, "particle_car": args.particle_car,
              "particles_visible": args.particle_car is None or all(
                  b["image_sha256"] != samples[i * args.samples]["image_sha256"]
                  for i, b in enumerate(particle_baselines)),
              "accuracy_explained": all(s["pgxp"].get("over_one_unexplained", -1) == 0 and
                  s["pgxp"].get("over_one_divide", -1) == 0 and
                  s["pgxp"].get("over_one_sxy", -1) == 0 and
                  s["pgxp"]["over_one_pixel"] == sum(s["pgxp"].get("over_one_" + k, 0)
                      for k in ("ir_sz", "ir", "sz", "fraction")) for s in samples),
              "images_identical": len({p["sha256"] for s in samples for p in s["presents"]}) == 1,
              "native_images_identical": len({s["native_sha256"] for s in samples}) == 1,
              "drawn_samples": len(drawn), "effects_draws": effects_draws, "effects_backfires": backfires, "effects_simulations": simulations,
              "vertex_streams_identical": len(drawn) == len(samples) and
                  len({s["stream_sha256"] for s in drawn}) == 1}
    (args.out_dir / "pgxp-static-audit.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({k:v for k,v in report.items() if k != "samples"}, indent=2))
    return 0 if (report["particles_visible"] and report["accuracy_explained"] and report["images_identical"] and report["native_images_identical"] and
                 report["vertex_streams_identical"] and
                 (not args.require_effects or effects_draws > 0) and
                 (args.particle_car is None or (backfires > 0 and simulations == 0))) else 1


if __name__ == "__main__":
    raise SystemExit(main())

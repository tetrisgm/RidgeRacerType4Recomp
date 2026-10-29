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


def capture(index, directory, port):
    before = request({"cmd": "present_shot_seq"}, port)["seq"]
    path = directory / f"present-{index}.png"
    request({"cmd": "present_shot", "path": str(path)}, port)
    deadline = time.monotonic() + 30
    while time.monotonic() < deadline:
        status = request({"cmd": "present_shot_seq"}, port)
        if status["seq"] > before:
            if not status.get("wrote") or not path.is_file():
                raise RuntimeError(f"present capture failed: {status}")
            break
        time.sleep(0.05)
    else:
        raise TimeoutError("present capture did not complete")
    frame = request({"cmd": "frame"}, port)["frame"] - 1
    audit = request({"cmd": "pgxp_frame", "frame": frame}, port)
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
    return {"frame": frame, "image_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "native_sha256": hashlib.sha256(native_path.read_bytes()).hexdigest(),
            "triangles": len(stream), "stream_sha256": hashlib.sha256(
                json.dumps(stream, sort_keys=True, separators=(",", ":")).encode()
            ).hexdigest(), "pgxp": audit}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("out_dir", type=pathlib.Path)
    parser.add_argument("--port", type=int, default=4798)
    parser.add_argument("--samples", type=int, default=3)
    args = parser.parse_args()
    args.out_dir.mkdir(parents=True, exist_ok=True)
    samples = [capture(i, args.out_dir, args.port) for i in range(args.samples)]
    drawn = [s for s in samples if s["triangles"]]
    report = {"samples": samples,
              "images_identical": len({s["image_sha256"] for s in samples}) == 1,
              "native_images_identical": len({s["native_sha256"] for s in samples}) == 1,
              "drawn_samples": len(drawn),
              "vertex_streams_identical": len({s["stream_sha256"] for s in drawn}) <= 1}
    (args.out_dir / "pgxp-static-audit.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    return 0 if report["images_identical"] and report["vertex_streams_identical"] else 1


if __name__ == "__main__":
    raise SystemExit(main())

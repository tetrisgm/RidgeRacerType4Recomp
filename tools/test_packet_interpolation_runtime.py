#!/usr/bin/env python3
"""Audit an already-running hidden race; never changes PGXP or game settings.

PSX_PACKET_INTERPOLATION_AUDIT=1 enables the presenter collector. This checks
candidate provenance only, not rendered interpolation. Every unresolved or
unclassified primitive fails, including candidate appearances. The default
gate requires 45 distinct pairs. Visibility changes require independent proof;
key absence alone cannot certify the moving-scene match bar.
"""
import argparse
import json
import time
from pathlib import Path

from dbg import cmd


def audit(port, seconds, stationary=False, sample_limit=45):
    samples = {}
    start = time.monotonic()
    while time.monotonic() - start < seconds and len(samples) < sample_limit:
        response = cmd({"cmd": "packet_interpolation"}, port=port)
        if not response.get("ok"):
            raise RuntimeError(response)
        row = response["interpolation"]
        if row["sequence"]:
            samples[row["sequence"]] = row
        time.sleep(0.05)
    rows = list(samples.values())
    errors = []
    if len(rows) != sample_limit:
        errors.append(f"Expected {sample_limit} live packet pairs, observed {len(rows)}")
    if not any(row["matched"] for row in rows):
        errors.append("No matched 3D geometry was observed")
    if not stationary and any(r.get("mode") == "vertex_source_audit" for r in rows):
        if not any(r.get("source_moving", 0) for r in rows):
            errors.append("No moving source geometry was observed; paused samples cannot pass the driving gate")
    for row in rows:
        if row["failed"] or row["ambiguous"] or row["invalid"] or row["partial_provenance"]:
            errors.append(f"Packet {row['sequence']} has ambiguous or missing provenance")
        if row.get("new_source", 0) != row.get("verified_appearance", 0) or row.get("unexplained", 0):
            errors.append(f"Packet {row['sequence']} has {row['new_source']} new-source vertices without complete model-appearance classification")
        if row["unmatched"]:
            errors.append(f"Packet {row['sequence']} has unclassified unmatched geometry")
    return {"mode": "candidate_audit_not_presenter_acceptance", "samples": rows,
            "sample_count": len(rows), "errors": errors,
            "max_new_source": max((r.get("new_source", 0) for r in rows), default=0),
            "max_source_moving": max((r.get("source_moving", 0) for r in rows), default=0),
            "max_unmatched": max((r["unmatched"] for r in rows), default=0),
            "max_ambiguous": max((r["ambiguous"] for r in rows), default=0)}


def capture_pair(port):
    """Freeze once, then page owned provenance without holding guest/render state."""
    cmd({"cmd": "packet_interpolation", "capture": 1}, port=port)
    deadline = time.monotonic() + 5
    while True:
        row = cmd({"cmd": "packet_interpolation", "detail": 1}, port=port)["interpolation"]
        if not row["pending"]:
            break
        if time.monotonic() >= deadline:
            raise RuntimeError("No new geometry packet arrived for the capture")
        time.sleep(0.05)
    pair = []
    for older in (1, 0):
        first = cmd({"cmd": "packet_interpolation", "detail": 1, "older": older}, port=port)["interpolation"]
        records = first["primitives"]
        for start in range(8, first["count"], 8):
            page = cmd({"cmd": "packet_interpolation", "detail": 1,
                        "older": older, "start": start}, port=port)["interpolation"]
            if page["sequence"] != first["sequence"]:
                raise RuntimeError("Another client replaced the frozen capture")
            records.extend(page["primitives"])
        pair.append({"sequence": first["sequence"], "primitives": records})
    return pair


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=4804)
    parser.add_argument("--seconds", type=float, default=30)
    parser.add_argument("--stationary", action="store_true")
    parser.add_argument("--samples", type=int, default=45)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--capture-pair", action="store_true", help="Save one frozen provenance pair after the timed gate")
    args = parser.parse_args()
    if args.seconds <= 0 or args.samples < 2:
        parser.error("--seconds must be positive and --samples at least two")
    result = audit(args.port, args.seconds, args.stationary, args.samples)
    if args.capture_pair:
        try:
            result["frozen_pair"] = capture_pair(args.port)
        except (RuntimeError, OSError) as exc:
            result["errors"].append(f"Capture failed: {exc}")
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps({k: v for k, v in result.items() if k not in ("samples", "errors", "frozen_pair")}))
    print(f"Failures: {len(result['errors'])}")
    return 1 if result["errors"] else 0


if __name__ == "__main__":
    raise SystemExit(main())

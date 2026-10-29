#!/usr/bin/env python3
"""Audit an already-running hidden race; never changes PGXP or game settings.

PSX_PACKET_INTERPOLATION_AUDIT=1 enables the presenter collector. This checks
candidate provenance only, not rendered interpolation. --stationary requires
zero unmatched samples and is appropriate only when geometry is unchanged.
Moving-scene appearances cannot be distinguished from identity loss by this
counter alone. Ambiguous identities and invalid provenance always fail.
"""
import argparse
import json
import time
from pathlib import Path

from dbg import cmd


def audit(port, seconds, stationary=False):
    samples = {}
    start = time.monotonic()
    while time.monotonic() - start < seconds:
        response = cmd({"cmd": "packet_interpolation"}, port=port)
        if not response.get("ok"):
            raise RuntimeError(response)
        row = response["interpolation"]
        if row["sequence"]:
            samples[row["sequence"]] = row
        time.sleep(0.05)
    rows = list(samples.values())
    errors = []
    if len(rows) < 2:
        errors.append("No live packet pairs: enable the audit and enter a race")
    if not any(row["matched"] for row in rows):
        errors.append("No matched 3D geometry was observed")
    for row in rows:
        if row["failed"] or row["ambiguous"] or row["invalid"] or row["partial_provenance"]:
            errors.append(f"Packet {row['sequence']} has ambiguous or missing provenance")
        if stationary and row["unmatched"]:
            errors.append(f"Stationary packet {row['sequence']} has unmatched geometry")
    return {"mode": "candidate_audit_not_presenter_acceptance", "samples": rows,
            "sample_count": len(rows), "errors": errors,
            "max_unmatched": max((r["unmatched"] for r in rows), default=0),
            "max_ambiguous": max((r["ambiguous"] for r in rows), default=0)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=4804)
    parser.add_argument("--seconds", type=float, default=10)
    parser.add_argument("--stationary", action="store_true")
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    if args.seconds <= 0:
        parser.error("--seconds must be positive")
    result = audit(args.port, args.seconds, args.stationary)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps({k: v for k, v in result.items() if k not in ("samples", "errors")}))
    print(f"Failures: {len(result['errors'])}")
    return 1 if result["errors"] else 0


if __name__ == "__main__":
    raise SystemExit(main())

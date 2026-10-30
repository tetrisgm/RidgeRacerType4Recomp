"""Collect compact, reviewable receipts from local exclusive interpolation runs."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RUNS = {
    "pre_fix": ("interp-waits-clean", ("60-A", "60-B")),
    "post_fix_1": ("interp-waits-after-retry", ("60-A", "60-B")),
    "post_fix_2": ("interp-waits-final", ("60-A", "60-B")),
    "post_fix_cpu": ("interp-waits-clean-cpu-retry", ("60-A", "60-B", "120-A", "120-B")),
}
KEYS = ("name", "seconds", "guest_vblanks_s", "starved_samples",
        "distinct_lower_bound_s", "presented_s", "guest_cpu_ms",
        "max_interval_ms", "interference", "failed_pairs_delta",
        "max_unmatched", "max_storage_misses")


def main():
    evidence = {
        "date": "2026-10-29",
        "conditions": "Hidden 7668x4320 race, full PGXP, Car Effects and Rewind enabled; A FPS off, B generic FPS on; fresh process and 60-second measurement per arm; 8x8 hash distinct-image lower bound; output 60 or 120 Hz",
        "note": "Arms with nonempty interference are rejected. Baseline A stall belongs to phase 3. Worst20 are per guest VBlank.",
        "runs": [],
    }
    for run_id, (directory, names) in RUNS.items():
        arms = []
        for name in names:
            base = ROOT / "analysis" / directory / name
            source = json.loads(base.with_suffix(".json").read_text())
            summary = source.get("summary", source)
            arm = {key: summary.get(key) for key in KEYS}
            arm["audited_pairs"] = summary["interpolation"]["interpolation"]["audited_pairs"]
            worst_path = base.with_name(name + "-worst20.json")
            if worst_path.exists():
                arm["worst20"] = json.loads(worst_path.read_text())
            arms.append(arm)
        evidence["runs"].append({"id": run_id, "arms": arms})
    output = ROOT / "docs/evidence/frame-interpolation-wait-parity-20261029.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(evidence, indent=2) + "\n")


if __name__ == "__main__":
    main()

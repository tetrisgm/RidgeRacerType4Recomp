"""Collect OS-accounted thread receipts without game data or savestate payloads."""
import json, re, statistics
from pathlib import Path

root = Path(__file__).resolve().parents[1]
base = root / 'analysis/release-waits'
runs = {}
for name in ('debug-120', 'release-120', 'fixed-120'):
    folder = base / name
    if not (folder / 'summary.json').exists():
        continue
    summary = json.loads((folder / 'summary.json').read_text())
    samples = json.loads((folder / 'samples.json').read_text())
    gpu = [float(s['gpu'].split(',')[0]) for s in samples if s['gpu']]
    summary['device_gpu_utilization_percent'] = statistics.mean(gpu)
    summary['device_gpu_utilization_peak_percent'] = max(gpu)
    summary['game_machine_cpu_percent'] = summary['game_cpu_core_percent'] / 16
    summary['estimated_background_cpu_percent'] = summary['host_cpu_percent'] - summary['game_machine_cpu_percent']
    rows = []
    for line in (folder / 'runtime.log').read_text(errors='replace').splitlines():
        if line.startswith('psxrecomp: frames='):
            rows.append({k: float(v) for k, v in re.findall(r'(\w+)=([0-9.]+)', line)})
        elif rows and line.startswith('psxrecomp: packet_'):
            rows[-1].update({k: float(v) for k, v in re.findall(r'(\w+)=([0-9.]+)', line)})
    b = rows[-1]
    start_ms = b['elapsed_ms'] - summary['report_seconds'] * 1000
    a = min(rows, key=lambda r: abs(r['elapsed_ms'] - start_ms))
    for key in ('packet_wait_ns', 'packet_restage_ns', 'packet_restages', 'packet_gpu_dropped', 'packet_gpu_samples'):
        if key in b:
            summary[key + '_delta'] = b[key] - a[key]
    runs[name] = dict(summary=summary,
                      threads=json.loads((folder / 'threads.json').read_text()),
                      provenance=json.loads((folder / 'provenance.json').read_text()),
                      raw_directory=str(folder))
target = root / 'psxrecomp/docs/internal/RELEASE_WAIT_RECEIPTS.json'
target.write_text(json.dumps(dict(cpu_percent_basis='100% = one logical CPU; 16 logical CPUs',
                                 runs=runs), indent=2) + '\n')
for name, run in runs.items():
    print(name, json.dumps(run['summary']))

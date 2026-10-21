# BRIEF-4 results

Stage 1 accepted on `fix/slowdowns` in both repositories. Production Release,
debug tools off, fullscreen 7680x4320 at 60 Hz, default Ultra, recorded route,
slot 2. Completion clock throughout; measurements start at route frame 900.

| Stage 1 change | Load | Completion Hz | Gaps >25 ms | Max gap ms | p99 emulation busy |
|---|---|---:|---:|---:|---:|
| Before resolution cap (27x / HiW) | quiet | 46.671 | 814 | 63.131 | 47.7% |
| Before resolution cap (27x / HiW) | 23.62% host CPU | 45.205 | 816 | 60.388 | 39.4% |
| 8K cap + SSAA threshold (18x, full VRAM) | quiet | 60.000 | 6 | 39.739 | 45.1% |
| 8K cap + SSAA threshold (18x, full VRAM) | 23.36% host CPU | 60.000 | 7 | 46.054 | 51.5% |
| Remove PGXP depth GL state queries | quiet | 60.001 | 8 | 70.377 | 53.2% |
| Exclude host waits from GPU query spans | quiet | 60.000 | 9 | 72.229 | 48.5% |
| Exclude host waits from GPU query spans | 23.44% host CPU | 59.998 | 5 | 52.780 | 60.6% |
| Exclude presenter pacing from dynres backpressure | quiet | 60.020 | 4 | 75.059 | 48.0% |
| Exclude presenter pacing from dynres backpressure | 23.34% host CPU | 60.009 | 4 | 72.844 | 50.0% |
| Reuse storage + corrected pacing accounting | quiet | 59.990 | 0 | 20.775 | 44.6% |
| Reuse storage + corrected pacing accounting | 23.51% host CPU | 59.956 | 0 | 21.173 | 48.8% |
| Repeat after final cap policy checks | quiet | 60.004 | 0 | 19.732 | 46.4% |
| Repeat after final cap policy checks | 23.35% host CPU | 59.957 | 3 | 66.734 | 55.4% |
| Event-published drawable dimensions | quiet | 59.856 | 36 | 105.705 | 70.6% |
| Event-published drawable dimensions | 23.28% host CPU | 59.990 | 0 | 22.035 | 55.9% |
| Remove production PGXP diagnostic copies | quiet | 59.990 | 2 | 38.060 | 50.5% |
| Coverage-only filtering + independent sleep timers | quiet | 59.973 | 0 | 20.266 | 46.1% |

Storage-reuse experiment: 59.940 Hz, zero gaps >25 ms, 21.836 ms maximum,
45.6% p99 busy. This isolated experiment fell to 4x under the old presenter
accounting. With corrected accounting, both later runs stay at 17x..18x.
Patch and receipts remain local under `analysis/brief4/local/dyn-storage*`.

Receipts: local `analysis/brief4/local/baseline/*-native-summary.json`, matching
CSV/stdout and load receipt. Native overlay calls positive, interpreted calls zero.
`state-proof.json` proves the master-compatible private slot and prescribed
`production-counter-saves` slot have identical bytes except header offsets 32..35;
the guest/device payload SHA-256 is
`fff644dbaf37984be1066170131c2d887f211af8220ac5b3cd50e7de8eefd1a4`.
Cap receipts: local `analysis/brief4/local/cap/{quiet,loaded}*`. Original saves and player settings remain untouched.

Accepted evidence: `coverage-production/quiet-summary.json` and
`drawable-events/loaded-summary.json` under local `analysis/brief4/local/`.
Rates round to 60.0 Hz; zero gaps over 25 ms, p99 busy below 60% in both.
The loaded case passed before the final production-only PGXP, coverage and
Windows timer fixes; only failing quiet was repeated, per the owner rule.
All runs have native overlay calls, zero interpreted/dirty execution and no
dispatch misses. Title ctest: 21/21 pass (`stage1-title-tests.txt`).
50 generated shards unchanged (`coverage-regen.txt`). The final binary uses the
prescribed production-counter save directly; `pgxp-production/state-proof.json`
confirms the same guest/device payload. Prior Windows/Mac component checks are
archived locally; no extra experiments were run after the minimum-testing note.

## Stage 2 (not accepted)

Local `fix/draw-distance` branch pair, from master with accepted Stage 1 carried
forward. R4 `e7c0677` implements full-circuit visibility (105 sections in the
slot), 65,535 range with qualified exact PGXP, 8 MB RAM/1 MiB heaps, HUD bounds
and per-merge visibility caching. Framework runtime source is identical to
`4b2fdfad` after removing unsuccessful later optimizations. Neither Stage 2
branch is pushed; Stage 3 has not started because the stages are ordered.

| Stage 2 change | Load | Completion Hz | Gaps >25 ms | Max gap ms | p99 emulation busy |
|---|---|---:|---:|---:|---:|
| Stage 1 baseline | quiet | 59.973 | 0 | 20.266 | 46.1% |
| Stage 1 baseline | 23.28% host CPU | 59.990 | 0 | 22.035 | 55.9% |
| Full circuit/range + exact far projection | quiet | 59.990 | 1 | 28.281 | 68.7% |
| Full circuit/range + exact far projection | 23.32% host CPU | 59.986 | 3 | 40.563 | 72.7% |
| Visibility + material caches | quiet | 59.953 | 3 | 37.317 | 65.3% |
| Visibility + material caches | 23.44% host CPU | 59.990 | 2 | 33.539 | 73.8% |
| Opaque bloom coverage fusion (current source) | quiet | 60.000 | 1 | 25.498 | 69.7% |
| Opaque bloom coverage fusion (current source) | 23.28% host CPU | 60.036 | 3 | 29.718 | 72.8% |
| Indexed decode + diagnostic receipt removal (removed) | quiet | 60.000 | 1 | 25.316 | 72.4% |
| Indexed decode + diagnostic receipt removal (removed) | 23.24% host CPU | 59.973 | 10 | 38.573 | 80.4% |
| Dead PGXP ALU clears (removed) | quiet | 60.020 | 14 | 34.190 | 92.8% |
| Dead PGXP ALU clears (removed) | 22.81% host CPU | 60.000 | 6 | 30.455 | 93.1% |

Receipts under local `analysis/brief4/local/`: `stage2-final/`, `stage2-cache/`,
`stage2-bloom/`, `stage2-decoded/`, `stage2-producer/`. Only failing timing cases
were repeated after code fixes. A subsequent noinline adjustment was removed
without another run; it did not reduce the enlarged binary. No ablation runs.
The producer warm-up sampler ended at frame 103, before measurement starts at
900 (`warmup-only.json`); its stacks show cycle stepping, GTE and PGXP work.
All receipts show native overlays, zero interpreted/dirty execution and no
reported dispatch/segment misses. Full-circuit timing remains over the bar.

Matching 8K captures at route 800/1202 (1202 holds left steering) are in
`stage2-before-turn/` and `stage2-cache/`; `comparison.jpg` verifies the expanded
heap HUD repair. They do not isolate the reported building motion, so the
IR-saturation correction is not claimed to fix that specific report. A
landmark/frame clarification is pending; no further exploratory captures are
made under the minimum-testing rule. Both timing and visual gates remain open.

The private slot extends RAM with 6 MiB of zeros, preserves the original 2 MiB
and every other device section, and updates compatibility cookies for the
selected execution/RAM map (`state-proof.json`). Original saves are untouched.
Title ctest: 21/21 on Windows and the isolated Mac title build. Final source
checks: `stage2-restored-title-tests.txt` on both hosts. Generated shards remain
unchanged. No PR, merge or release.

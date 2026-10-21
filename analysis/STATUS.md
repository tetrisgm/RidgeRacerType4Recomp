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

## Stage 2 (in progress)

`fix/draw-distance` branch pair, from master with accepted Stage 1 carried
forward. Raise the course ceiling from 32 to all 105 loaded sections and the
PGXP-qualified range from 30,000 to 65,535; retain guest-work/heap guards.
Validated intact GTE words carry exact IR-unclamped X/Y; altered/half words
and fallback caches cannot inherit that privilege. R4 enables 8 MB RAM and
1 MiB primitive heaps; the HUD packet bounds support both heap layouts.

| Stage 2 change | Load | Completion Hz | Gaps >25 ms | Max gap ms | p99 emulation busy |
|---|---|---:|---:|---:|---:|
| Stage 1 baseline | quiet | 59.973 | 0 | 20.266 | 46.1% |
| Stage 1 baseline | 23.28% host CPU | 59.990 | 0 | 22.035 | 55.9% |
| Full-circuit/range + exact far projection | quiet | 59.990 | 1 | 28.281 | 68.7% |
| Full-circuit/range + exact far projection | 23.32% host CPU | 59.986 | 3 | 40.563 | 72.7% |
| Per-merge visibility + material-command caches | quiet | 59.953 | 3 | 37.317 | 65.3% |
| Per-merge visibility + material-command caches | 23.44% host CPU | 59.990 | 2 | 33.539 | 73.8% |

Initial timing receipts: local `analysis/brief4/local/stage2-final/`.
Both initial runs and their cache-fix repeats failed. Opaque bloom-source
coverage fusion is applied before the next failing-case repeats. Per-merge
visibility caching reduces cone decisions
about 16x with identical block counts and guest-cycle totals in the visual trace.
The private save extends RAM with 6 MiB of zeros, preserving the original 2 MiB
and every other section; `state-proof.json` records the section hashes and
compatibility-cookie changes. Original saves are untouched.

Matching fullscreen visual captures target route 800 and 1202 (left steering),
under local `stage2-before-turn/` and `stage2-cache/`. The first visual after
capture exposed a HUD range regression, fixed before timing acceptance.
Title ctest: 21/21 Windows and 21/21 isolated Mac build; receipts
`stage2-title-tests-fixed.txt` on each host. The 800/1202 images verify the HUD repair but do not isolate the reported
building drift; that visual acceptance remains open. No Stage 2 acceptance yet.
Stage 3 waits for Stage 2.

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

## Stage 2

Owner decision: exactly **40 sections ahead**, one behind, in Maximum. No
ceiling search continues. Stage 1 settings, full-depth PGXP and conservative
pre-GTE visibility rejection remain. The fixed default does not step down
under load. Course RAM is 8 MB; the private prescribed slot zero-extends RAM
without changing the original guest/device payload.

The post-tunnel turn is route frame 2400. Master has 60 consecutive fullscreen
frames with unchanged distance, plus the requested PGXP-off/widescreen-off
controls. The gross skyline shift survives both controls. The correction
preserves positive unwrapped MAC3 through validated PGXP dataflow and GPU
perspective/depth encoding; guest GTE registers and legacy u16 depth APIs stay
unchanged. The rejected 65,535 far-plane cull hid visible towers and never
qualified as a pass. Full depth restores their correct far projection.

| Stage 2 change | Load | Completion Hz | Gaps >25 ms | Max gap ms | p99 busy |
|---|---|---:|---:|---:|---:|
| Full depth, full circuit, flat fine culling | quiet | 59.874 | 16 | 30.951 | 91.7% |
| Same | moderate CPU | 54.929 | 287 | 40.882 | 80.8% |
| Owner-fixed 40 ahead | quiet | 60.000 | 0 | 22.851 | 56.0% |
| Same | 23.39% host CPU | 60.000 | 1 | 25.641 | 61.5% |
| Submit before pacing (reverted) | moderate CPU | 60.000 | 8 | 31.057 | 71.4% |
| Match Windows producer priority | moderate CPU | 60.000 | 0 | 24.455 | 61.6% |
| Inherit proven planes (reverted) | moderate CPU | 60.003 | 2 | 26.242 | 73.7% |
| Bulk queued uploads + matched priority | moderate CPU | 60.000 | 0 | 23.013 | 58.7% |

Accepted timing receipts (local `analysis/brief4/local/`):
`stage2-fixed40/quiet-*` and `stage2-fixed40-upload/loaded-*`. Only failing
loaded was repeated after cost fixes, per the minimum-testing rule. Both have
positive native overlay counts (69,996 / 69,998), zero interpreted/dirty
execution and no dispatch/segment misses. Windows and isolated Mac title ctest
pass 21/21 (`stage2-fixed40-upload-title-tests.txt`). Mac production PGXP
numeric checks also pass. Bulk uploads reuse `910eef11` without its unrelated
coverage changes; failed pacing and plane-traversal experiments are removed.

Visual receipts: `master-corner-2400/` (before and controls),
`stage2-facing-turn-2400/` (full-depth after; 60 consecutive native frames),
and `stage2-fixed40-turn-2400/` (final 40-section after: frames 2398..2457,
60 consecutive, 38,404 native calls, zero interpreted calls/misses). The hierarchy and
backface-culling captures were pixel-identical at the first/last frames.
Late grey-roof tracking was occluded by a foreground pole; its correlation
jump is not building drift evidence.

Stage 2 accepted; final pin: framework `8130e6fd`. Regeneration leaves all
50 game shards unchanged. Stage 3 remains next. No PR, merge or
release is authorized. See the owner brief for the ordered acceptance gates.

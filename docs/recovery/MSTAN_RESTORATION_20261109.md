# MStan performance restoration — 2026-11-09

Four preserved performance candidates are now integrated into engine
`b57e486e`, alongside the custom menu and Steam Controller fixes. The prior
claim that all MStan performance work was retained was too broad: the November
2 recovery froze upstream `c1f5cafd`, while its candidate ledger deliberately
kept additional optimizations on separate branches. The consolidation did not
finish their integration. The previous Mac comparison used two builds missing
these candidates and therefore could not establish their preservation.

## Restored and checked

| Candidate | Preserved source | Landed commit |
|---|---|---|
| Batched render queue publication / separate cache lines | `e57db5da` | `faf7bcb3` |
| Cached GL blend/stencil state | `63677cb7` | `7330aa66` |
| Fixed shader sampler units | `cc413ceb` | `b1bfaa79` |
| Allocation-free HD lookup misses / delayed draw leases | `b7f91c6e` | `7531a8a7` |

These are the independent candidates MStan evaluated from the older fork;
not all were originally authored by him. His ledger is engine commit
`88496e72`, `docs/R4_PERFORMANCE_CANDIDATES.md`. The independent source changes
retain their original authorship and cherry-pick provenance.

## Remaining work reviewed

The [disposition receipt](MSTAN_RESTORATION_20261109.json) accounts for all six
small candidates, three larger dependency stacks, two inconclusive small
changes and later framework leads. All 13 MStan-authored game commits are
ancestors of the current game. The 46 later MStan-authored upstream framework
commits were inventoried against upstream `cddb3221`; this is not a claim that
every later framework feature was individually ported or runtime-qualified.

- Disc prefetch remains experimental, as recorded by MStan: CHD, CD-DA and
  mount lifetime qualification are incomplete, with no demonstrated benefit
  for this racing render bottleneck.
- Windows render-thread priority remains unqualified on its target platform.
  Mac testing cannot justify raising its shipping default.
- Old draw-context, packet/presentation and rewind-worker architectures remain
  preserved research. Their dependencies and unresolved complete-frame checks
  prevent treating them as independent optimizations.
- Triangle grouping and display-layout caching have inconclusive historical
  measurements. Two earlier DuckStation-pack changes were already covered by
  upstream per MStan's ledger and were empty on recovery replay.
- Later native-entry routing, resident guards, geometry sampling, startup
  hashing and watchdog changes are explicitly distinguished in the receipt.
  None is silently represented as included. The native-entry hook query changed
  in `06bf20f9` is absent from this pinned routing API; the resident guard bulk
  path has no current R4 call sites. A future upstream refresh needs its own
  codegen/ABI qualification rather than isolated dependency-blind hunks.

## 4K Mac results

The [measurement receipt](MSTAN_PERFORMANCE_20261109.json) records exact raw
runs and executable hashes. Measured engine `7531a8a7` and delivery pin
`b57e486e` have identical runtime sources; the latter adds the qualification log.
M4 / 16 GiB, fixed 9x/2,160-line rendering, 21:9,
PGXP, Max Detail, 8 MB RAM, HD HUD and effects retained. Two 30-second eight-car
start runs per build, alternating baseline/candidate/candidate/baseline:

| Build | Run 1 VBlanks/s | Run 2 VBlanks/s | Mean |
|---|---:|---:|---:|
| Before restoration (`d585a728`) | 46.087 | 45.706 | 45.896 |
| Four candidates restored | 47.600 | 47.013 | 47.306 |

That is about **3.1% higher game speed in this paired test**, not full speed.
The result does not attribute a separate gain to each candidate. A diagnostic
bounded high-resolution target trial was slower (44.77 VBlanks/s) and was
rejected; shipping target policy and owner settings are unchanged.

Substantial background work remained active. Hidden/dummy-audio tests used
process-local driver VSync off to avoid the observed hidden-window Cocoa swap
wait, while keeping normal game-speed pacing. Presentation telemetry is API
completion, not physical scanout. The same starting save and closed-loop driver
were used, not identical guest-frame input trajectories. Windows/NVIDIA benefits
cannot be inferred from these Mac measurements.

## Verification and delivery

- Mac Release and 29/29 game tests pass.
- Render-ring ordering/lifetime tests: 8,843 checks in each publication mode.
- Six real-GL cases match readback/native/high-resolution/wide hashes with
  batching/caches enabled and disabled. HD core and real-GL replacement tests
  pass; the HD visual hashes match the earlier pre-restoration receipt.
- Actual custom menu: Select/Back navigation, paused gameplay, bindings,
  devices/deadzones/volume, save/load and restart persistence pass. UI pin
  `ef8150f` is unchanged; runtime menu/controller source is unchanged by this port.
- Forty-step smoke reaches frame 8,477, with 7,173,065 native overlay calls,
  zero dispatch/segment misses and zero interpreted R4 overlay PCs.
- Supported regeneration reuses all 7,459 audited native variants; generated
  C remains unchanged. No native execution optimization was disabled.
- Windows x64 cross-build succeeds on the Mac. New portable candidate:
  `dist/windows/2026-11-09-mstan-restored-01/`. Packaging now also validates and
  bundles the executable's execution-contract sidecar before checksumming.

Raw local evidence is under `analysis/mstan-restoration-20261109/`.
NAS, STEAMBOX and native player profiles were not accessed. No public release
was published. The four source fixes have independent upstream reviews listed
in [UPSTREAM.md](../UPSTREAM.md).

**Still open:** consistently full-speed 4K, visible-window pacing/audio,
all-course qualification and physical controller/Windows play. Restoration is
qualified; the broader 4K performance problem is not marked fixed.

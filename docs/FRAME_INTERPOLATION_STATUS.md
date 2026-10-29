# Phase 4 topic status — not accepted

Worktree: `r4-wt-interp`, branch `feat/frame-arch-4-interp`, based on
`feat/frame-arch-5-rewind` at `21d2a50`. Framework topic branches from fork
`origin/r4/main` at `d2185504`.

The framework supplies an owned packet-pair sampler, PGXP projection/candidate
provenance transport and an opt-in TCP audit. See
[the framework status and evidence](../psxrecomp/docs/PACKET_INTERPOLATION.md).

The match gate fails: a moving-race diagnostic found up to 332 unmatched and
32 ambiguous primitives across 160 sampled packets. Caller/model coordinates
do not uniquely identify repeated instances. No production FPS cutover,
Car Effects metadata interpolation, legacy deletion or generic 8K60/120
acceptance is claimed.

The explicit runtime gate subsequently failed 18 of 45 samples (up to 652
unmatched and 66 ambiguous), recorded in `analysis/interp-regression-gate.json`.

Validation: R4 build and 32/32 CTests; five focused framework CTests;
hidden complete-frame GL fixture; hidden smoke through frame 9225 with zero
dispatch/segment misses. PGXP stayed on. All game windows were hidden.

Local-only receipts are under `analysis/interp-smoke/`,
`analysis/interp-candidate-audit.json`, and `analysis/interp-candidate-8k.json`.
The latter records an 18x race with full PGXP, Rewind enabled and 329 actual
Car Effects draws. It measures the old presenter, not the new sampler. No
distinct-image rate or starved-sample count has been established for the new
path. The temporary rate summary includes diagnostic-query overhead and is
not an acceptance benchmark.

Reproduce candidate checks on a running hidden race with:

```
python tools/test_packet_interpolation_runtime.py --port 4804 --seconds 10 --out analysis/interp-audit.json
```

Start with `PSX_PACKET_INTERPOLATION_AUDIT=1`. The script fails on ambiguous
or missing provenance. `--stationary` additionally fails on any unmatched
geometry, for a scene known to have unchanged geometry. This cannot certify
moving-scene visibility changes or output pixels.

The shared checkout and its build directory were not modified or used.
Submodules and regeneration/build outputs belong to this worktree. The
launcher remote lacked its pinned commit; the exact commit was fetched from
the existing local repository into this worktree's submodule.

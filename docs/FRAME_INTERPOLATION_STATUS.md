# Phase 4 interpolation status: incomplete, not accepted

Worktree: `C:/Users/Shokunin/dev/r4-wt-interp`. Both repositories remain on
`feat/frame-arch-4-interp`. Main checkout and its build directory were untouched.
No merges or R4 PRs were opened.

## Implemented continuation

- Original vertex RAM origins survive exact full-word copies through scratchpad;
  partial writes invalidate the origin. Matrix RAM origin, transform fingerprint,
  primitive type, texpage and CLUT accompany the existing PGXP provenance.
- Equal-count duplicate groups pair k-th to k-th. Unequal groups use deterministic
  gated minimum-cost assignment, with separate resolved-group counters.
- Frozen packet-pair TCP inspection exposes model coordinates, origins, transform
  fingerprints and view coordinates without accessing live guest/GL state.
- The near-plane/projective draw path now participates in the audit. It was
  omitted from the first pass. Its signed-depth inputs are covered by a fixture.
- The runtime gate requires 45 distinct samples and rejects every unclassified
  unmatched primitive. Candidate appearances are not accepted merely because
  their keys are absent from the previous packet.

## Results

| Gate | Result |
| --- | --- |
| R4 build with JOBS=4 | Passed |
| R4 CTest | 32/32 passed |
| Focused framework tests | 5/5 passed |
| Regeneration | All 50 shards unchanged |
| Hidden smoke/navigation | Zero dispatch and segment misses |
| Ordinary-path-only moving census | Failed 35/45; max 448 unmatched, 0 ambiguous |
| Inclusive projective census | Failed 45/45; max 1592 unmatched, 0 ambiguous, 1 invalid, 0 partial provenance |

The inclusive gate follows turbo boot navigation and includes its initial
transition pairs. It is not a performance benchmark. Runtime receipts confirm
18x/4320 lines, full PGXP, enabled Rewind and active Car Effects. Three frozen
ordinary-path pairs had 30/32/16 missing keys and no equal ordered model-space
triangle under a different key. That suggests changing visible geometry/topology;
it does not prove genuine appearance/disappearance or no-pop behavior.

Local-only evidence:

- `analysis/interp-strengthened-final-45.json`
- `analysis/interp-inclusive-45.json`
- `analysis/interp-inclusive-pair.json`
- `analysis/interp-inclusive-configuration.json`
- `analysis/interp-final-pair-{0,1,2}.json`
- `analysis/interp-inclusive-race.png` and `interp-inclusive-preview.png`
- `analysis/interp-final-smoke/`

Reproduce after entering a hidden race with `PSX_PACKET_INTERPOLATION_AUDIT=1`:

```
python tools/test_packet_interpolation_runtime.py --port 4808 --seconds 30 --samples 45 --capture-pair --out analysis/interp-gate.json
```

## Outstanding work

The positive-depth sampler cannot yet handle all signed near-plane geometry.
Stable model invocation/primitive ordinals and genuine visibility classification
are still unproven. A zero-unmatched claim would currently conceal these gaps.

Production presenter integration, no-pop transitions, packet-owned Car Effects
interpolation, R4 FPS cutover, and deletion of guest re-execution/checkpoints/
transactions/private workers remain undone. No generic 8K60/120 distinct-image,
guest-VBlank or starved-sample measurements exist. Legacy interpolation remains.

All game windows were hidden, PGXP stayed enabled, and all processes started
for these runs were stopped. See the framework's
[implementation notes](../psxrecomp/docs/PACKET_INTERPOLATION.md).

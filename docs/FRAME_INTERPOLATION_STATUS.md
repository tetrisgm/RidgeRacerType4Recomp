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


## Vertex-source continuation (2026-10-29)

Framework topic at `657876f2` now captures resolved RTPS/RTPT sources before guest culling,
including culled faces' vertices. Each packet owns a bounded history attachment,
so dropping a mailbox packet does not lose its only copy of source provenance.
The independent sampler matches individual sources, retains N's HUD/UV/colour,
accepts signed depth and emits homogeneous coordinates. A separate view-space
near-plane clipper avoids division at zero/negative depth. Main PGXP timeline
invalidation clears the producer collector; private contexts do not publish.

Validation before the final identity/lifecycle follow-up:

- 45 **driving** packet pairs: zero source-unmatched, zero ambiguous, zero
  invalid; maximum 2274 matched vertex uses per pair.
- Three pairs failed the strengthened gate with 19, 31 and 36 unverified
  new-source vertex uses. A new-source counter is not proof of a genuine spawn.
- Earlier zero-miss 45-pair receipts were **paused** (confirmed by captures),
  so they are only stationary repeatability evidence.
- Build with JOBS=4 and R4 CTest: 32/32 passed. Focused sampler/stream fixtures
  pass, including signed-depth clipping, mutable vertex values, cross-VBlank OT
  submission and immutable attachments across twelve discarded publications.
- Regeneration: 50 shards unchanged. Hidden navigation: zero dispatch/segment
  misses. Local receipts: `analysis/interp-source-live45.json`,
  `analysis/interp-source-smoke3/`, and subsequent `interp-source-final-*` files.

The collector currently resolves transform groups referenced by submitted
geometry. Entirely culled model invocations and occurrence identity when visible
instance counts change are not proven. The runtime gate rejects unclassified
new sources and retains diagnostic results when capture fails. The original
single invalid-depth case has not been conclusively reproduced/explained; the
new signed-depth fixtures establish the mathematical path only.

**Phase 4 remains incomplete.** Presenter cutover, packet Car Effects sampling,
legacy guest-redraw/checkpoint/worker deletion, R4 FPS switch, no-pop captures,
and generic-path 8K60/120 distinct-image/VBlank/starvation measurements remain
outstanding. No generic performance result is claimed. All edits/builds stayed
in this worktree; no main-checkout build, merge, PR or visible window was used.


Latest ordinal follow-up: shared RAM staging addresses now get a pre-cull
vertex ordinal; the fixture changes model coordinates without changing source
order. Broader steering census `analysis/interp-ordinal45.json` failed 27/45
pairs, max 617 new-source uses, max 2394 matched uses, zero source-unmatched,
ambiguous and invalid. Configuration receipt confirms 4320 lines, full PGXP,
Rewind enabled, zero dispatch/segment misses. It predates the final
`source_moving` diagnostic, so `--stationary` disabled motion enforcement for
that census; it is not a no-pop or driving acceptance result. The final harness
now explicitly rejects stationary samples in its default driving mode.
Final sampler/stream fixtures and build pass. All owned game processes stopped.

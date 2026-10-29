# Phase 4: presenter cutover implemented; acceptance still open

Worktree: `C:/Users/Shokunin/dev/r4-wt-interp`; both repositories use
`feat/frame-arch-4-interp`. Main checkout/build untouched. No merges or PRs.

## Implementation

- Generic retained packet presenter samples view-space geometry and projection
  constants at rational 60/90/120/144 deadlines; no guest re-execution.
- Stable source/primitive keys ignore OT packet addresses and material animation.
  Shared staging addresses are identified across callers and use model points,
  avoiding ordinal shifts when subdivision order changes.
- Whole-model appearance classification: absent invocation means verified
  appearance at N; a missing point in an existing invocation uses its retained
  GTE affine transform. Ambiguous or unexplained correspondence fails the census.
- Car Effects camera/car/light metadata is owned by packets and interpolated on
  the render owner. It never selects a serial guest-render path.
- R4 FPS selects generic interpolation. Legacy `r4_interp` code, fields and field
  generator, private workers/adapters and their tests were removed. Regeneration
  replaced 468,936 lines of private draw clones with an empty 39-line emitter
  result; all 50 ordinary shards are unchanged.
- Shared framework transaction APIs remain for independent netplay local-view
  and stereo/VR rendering. R4 FPS has no checkpoint/transaction dependency.
- GTE affine matrices now live in observer attachments, not every PGXP shadow,
  avoiding extra rewind/savestate memory traffic. Native source-key updates no
  longer flush unrelated GPU geometry batches.

## Validation

R4 Release build with JOBS=4 and 23/23 remaining CTests pass. Fresh sampler,
stream, presenter/metadata and mailbox/lease fixtures pass. Cross-caller staging
and repaired hidden GL ownership fixtures are being rerun on the final revision.

The previous 10-second 8K race (18x, full PGXP, Car Effects and Rewind enabled):

| Requested output | Distinct images/s | Guest VBlanks/s | Starved samples | Max guest interval | Max output interval |
| --- | ---: | ---: | ---: | ---: | ---: |
| 60 | 22.1 | 48.1 | 87750 | 60.701 ms | 123.783 ms |
| 120 | 18.9 | 43.4 | 121289 | 61.962 ms | 374.202 ms |

45-pair census before and after rewind: zero unexplained/unmatched/ambiguous;
204/204 appearances verified in the initial census. Rewind restored 57 snapshots
with zero failures/misses, reset the interpolation sequence, and resumed cleanly.
Car Effects draws increased 221/190; dispatch misses were zero.

These numbers predate compact observer metadata and cross-caller staging.
Their 120-Hz captures still contain intermittent world seams, so they DO NOT
close visual/no-pop acceptance. Fresh final-revision race validation is underway.
The 60/120 distinct-images-per-second performance gate is not met.

Evidence is local-only under `analysis/interp-final/`; final follow-up receipts
will be under `analysis/interp-compact/`. Census counters alone are never claimed
as proof of smooth output or correct visibility transitions.

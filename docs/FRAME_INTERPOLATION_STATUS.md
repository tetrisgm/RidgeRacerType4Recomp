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
stream, presenter/metadata and mailbox/lease fixtures pass, including cross-caller
staging. Hidden real-GL complete-frame ownership fixture: 111 checks, 0 failures.

Ten-second hidden 8K races (18x, full PGXP, Car Effects and Rewind enabled),
compact observer layout at framework `2e39cbe6`:

| Requested output | Distinct images/s | Guest VBlanks/s | Starved samples | Max guest interval | Max output interval |
| --- | ---: | ---: | ---: | ---: | ---: |
| 60 | 25.6 | 46.5 | 97296 | 60.282 ms | 130.216 ms |
| 120 | 26.1 | 47.2 | 92910 | 57.104 ms | 103.041 ms |
| Stock (generic rate 0) | 24.6 | 49.2 | 79172 | 52.923 ms | 100.539 ms |

45-pair census: zero unexplained/unmatched/ambiguous. Car Effects draw counts
increased 257/261 in the 60/120 runs. Dispatch misses were zero. Compact-layout
rewind restored 49 snapshots with zero failures/misses, cleared interpolation to
sequence 0, and passed a fresh 45-pair census after resuming.

Visual comparison FAILS at `2e39cbe6`: all 262 120-Hz images were inspected;
intermittent side-wall sky gaps and cyan roof strips remain. All 247 stock images
were inspected; corresponding walls and roof stay closed. Therefore these gaps
are an interpolation regression, not merely the stock renderer's throughput
problem. The 60/120 distinct-images-per-second performance gate is also not met.

Offline captured pairs identify ordinary near-camera motion triggering per-vertex
cut masks (79 candidate sources in the compact pair). An experimental whole-model
extent gate passed its focused fixture but made the live geometry worse: dark
missing-road regions and intermittent sky holes. The experiment was reverted;
it is not included in the final topic commits. This rules out accepting a simple
cut-threshold change as the fix. Exact incorrect correspondence/topology handling
remains unresolved; endpoint versus in-between attribution was not recorded.

## Integration disposition

Phase 4 is NOT accepted. The topic contains the presenter/FPS cutover and legacy
removal, but the prerequisite that generic output match or beat the old path has
not been established. Do not integrate this as a completed replacement. Remaining
work is to fix interpolation-only geometry gaps, verify true appearance and
disappearance transitions in captures, verify cuts/respawns with Car Effects,
and reach the requested 60/120 distinct-image rates without audio starvation.
Shared framework transaction APIs still serve netplay/stereo/VR; their wholesale
removal was not performed by this topic.

Evidence is local-only under `analysis/interp-compact/`,
`analysis/interp-stock-reference/` and `analysis/interp-model-cuts/`.
Census counters alone are never proof of correct visibility transitions.

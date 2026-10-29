# Phase 4 follow-up: geometry fixes pushed; performance acceptance FAILED

2026-10-29. Framework implementation through `d516f5fa` on
`feat/frame-arch-4-interp`. Latest fetched `origin/r4/main` is `d2185504`,
already an ancestor. Main checkout/build and the surfaces topic were untouched.

## Changes in this pass

- One render-owned source slot computes each interpolated view/projection/screen
  position once. Every referencing primitive copies identical geometry bits;
  colours and UVs remain per primitive. The ordinary raster hook now assigns the
  canonical position rather than adding it to each face's different PGXP residual.
  Existing PGXP seam processing runs after interpolation.
- Connected source and screen-edge components hold together when provenance is
  missing; a GTE-less neighbour cannot leave just one side of an edge moving.
  Local near-plane motion no longer triggers a partial model cut. Explicit
  timeline/cut flags and scene-wide discontinuities snap the whole pair.
- The interpolation topology includes departing triangles. Their current view
  positions come from retained pre-cull transforms; old/current faces use the
  same source table. Replay occurs after packet clears and before the final 3D
  primitive/HUD. Only departing faces restore old texture inputs, and current
  target, draw state, dithering and native texture inputs are restored afterward.
- Producer transform capture is a bounded array append, with no per-vertex map,
  allocation or history compaction. Packet boundaries share immutable history
  blocks. Rewind can retain overflow blocks without exhausting publication.
  Matching, source tables, visibility union and census run on the render owner.
  `PSX_PACKET_INTERPOLATION_AUDIT` is disabled by default.
- Repeated immutable HD tracking snapshots retain their identity and skip parsing
  when destination generations still match. The regression fixture verifies that
  a genuine destination mutation forces restoration.
- Exact endpoints replay the retained original packets, including topology and
  metadata. The debug server supports `packet_interpolation sample_milli=0..1000`
  and `screenshot packet=1` to capture the actual presenter target rather than
  the guest's other double buffer. Car Effects remains packet metadata/render work.

## Visual and identity evidence

45 moving pairs on the final implementation: **0 unmatched, 0 ambiguous,
0 partial provenance, 0 failed pairs**, up to 9,360 moving source uses.
Source keys are independent of OT/double-buffer addresses. Unit fixtures reverse
packet order/allocations and verify bit-identical shared positions, including an
adversarial one-ULP difference between two references to one source.

R4's observed track/wall census has complete provenance. The recorded track path
through caller `0x80065904` enters `0x80066154`, with RTPT at `0x800662B8`
(`generated/SLUS_007.97_full_29.c`, read only). No Ghidra requirement was imposed.

Three frozen moving pairs at 4/6/8 seconds after the starting-grid state were
captured at 7668 x 4320. The stock endpoint replay at t=0 and t=1 was repeated
around a t=0.5 image: **0 changed pixels** on either repeated endpoint in all three
pairs. Both endpoint paths bypass interpolation. This is retained-packet replay
validation, **not** an independently booted stock-executable comparison.

Clear-colour gap proxy: exact RGB (48,48,48), full width of the lower 45% of the
image (road/walls/HUD). Each endpoint and midpoint contained 21,590 such pixels;
**0 additional midpoint clear pixels** in all three final cases. Earlier failing
captures had millions of additional pixels. This proxy does not detect every
possible sky-coloured roof crack or prove all visibility transitions/cuts.
The broad visual acceptance bar therefore remains open.

Local evidence: `analysis/interp-pair-sweep-{4,6,8}/`,
`analysis/interp-delivery/census45.json`. The latest sampled retirement counters
were zero unresolved; the debug JSON now exposes `retiring` and
`retiring_unresolved` separately from matches present in both endpoints.

## Final performance measurement ? FAIL

Release, debug tools enabled for telemetry, JOBS=4, hidden window, port 4818,
18x/4320 internal lines (7668 x 4320 wide capture), full PGXP, Car Effects on,
Rewind on. Audit environment 0. Ten-second windows after a starting-grid state
load and three-second driving warmup. Distinct images are consecutive decoded
present-ring image hashes; all requested sequences fit the 600-image ring.

| Requested output | Distinct images/s | Guest VBlanks/s | Starved samples | Max guest interval | Max output interval |
| --- | ---: | ---: | ---: | ---: | ---: |
| 60 | 27.0 | 51.5 | 60,339 | 61.977 ms | 98.026 ms |
| 120 | 30.7 | 49.4 | 78,290 | 59.795 ms | 117.315 ms |

Car Effects draw deltas: 310 / 352. Final census misses: zero.
Evidence: `analysis/interp-delivery/{60,120}.json` and `summary.json`.
An earlier warmed run reached 60.1/60.4 guest VBlanks/s with zero starvation,
but it was **not reproducible**, so it is not the acceptance result. A late
rate-zero control also fell to 43.5 guest VBlanks/s; this does not establish the
supervisor's required 60.0 baseline under identical conditions.

Profiling removed a repeated HD tracking parse/allocation hotspot. Remaining
render work includes native texture inputs, CPU uploads, geometry and swap;
source matching is render-owned. Changing Windows multimedia priorities made
results worse and was reverted. A process-local power-throttling experiment did
not recover performance and is not in the code; that owned process was stopped.
No scheduling or host power-policy change is shipped.

## Validation and disposition

All five focused framework CTests pass, including the hidden real-GL packet
fixture, sampler, stream/retained-history, presenter/metadata and HD context.
R4's 22 permitted CTests pass. Regeneration left all 50 game shards and the empty
legacy draw-clone emitter unchanged. The renderer fixture gained its missing
projection-tracking stub. All reported race runs/captures keep PGXP full.

Validation correction: an initial broad CTest invocation inadvertently ran the
`r4_pgxp_boot` test's PGXP-off arm before its contents were inspected. That test
was excluded from every subsequent suite run; no PGXP-off reference was used.

**Do not integrate this as a completed phase 4.** Shared-vertex consistency and
sampled disappearing-geometry gaps are improved and covered by fixtures/captures,
but the mandatory stable 60.0 guest VBlanks/s / zero-starvation gate remains
unmet. Independent stock capture comparison, exhaustive roof/visibility checks,
and live cut/respawn validation are not established by the narrower tests above.
No R4 PR was opened and no topic was merged into either main branch.

---

## Previous pass (historical)

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
Final reverted-source rebuild and all 23 CTests pass. Hidden smoke exits 0 after
40 steps through race-12s (frame 9179), with PGXP enabled and zero dispatch/segment
misses. Evidence: `analysis/interp-reverted-final-smoke/`. All owned runtimes stopped.

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

# Phase 4 wait-parity follow-up — ready for supervisor integration (2026-10-29)

The generic presenter now passes the **B≈A guest-wait gate at 60 Hz** within the
large same-build baseline variance. The latest locked, interference-free 60-second
pair measured 56.163 guest VBlanks/s and 173,740 starved samples with FPS off (A),
versus 56.359 VBlanks/s and 159,957 starved samples with generic interpolation
on (B). B produced 49.23 distinct images/s (8x8-hash lower bound), with zero
unmatched primitives or storage misses across 1,441 audited pairs. Guest CPU
time was 43.14 s in A and 44.55 s in B, so this is not a claim that interpolation
has no CPU cost.

The pre-fix locked pair had B 2.080 VBlanks/s slower and 102,393 more starved
samples than A. Three clean post-fix 60 Hz pairs had B−A cadence differences
of +0.043, −1.722, and +0.196 VBlanks/s, and starvation differences of
+8,454, +78,962, and −13,783. The middle pair shows the residual variability:
its A arm itself fell from 59.142 to 56.239 VBlanks/s and rose from 25,516 to
167,452 starved samples. Median post-fix B−A is +0.043 VBlanks/s and +8,454
starved samples, against A's 142,000-sample range. This supports parity within
noise, not a guarantee that every individual run is faster.

Per-VBlank worst-20 telemetry records pacer sleep and overshoot, publication,
native raster lock/space/completion waits, history release, and guest CPU time.
In the latest clean B arm, native queue-full waits were 0 ms throughout its
worst 20 intervals; history release peaked at 0.0016 ms, publication at
0.6485 ms, native completion at 0.3481 ms, and sleep overshoot at 0.4719 ms.
The packet wake no longer takes the render worker's mutex. The presentation
mailbox remains latest-wins, and the guest returns history through a fixed-step
deferred queue while a worker reclaims blocks. The render consumer cannot retain
a history lease that makes the guest wait for reclamation.

The locked 120 Hz A arm was clean (56.820 VBlanks/s, 145,049 starved samples),
but a separate runtime overlapped 53 seconds of the 120 Hz B arm; the harness
rejected that pair and it is **not** used for a performance claim. Earlier
clean 120 Hz results reached 94.02 distinct images/s with zero unmatched
primitives across the full four-arm run. Full 8K throughput remains limited
by the FPS-off baseline stall owned by phase 3. Raw local receipts are in
`analysis/interp-waits-*`; the committed compact evidence includes all clean
and rejected arms plus worst-20 intervals in
[frame-interpolation-wait-parity-20261029.json](evidence/frame-interpolation-wait-parity-20261029.json).
The old failed acceptance report follows as historical context.

# Exclusive performance follow-up — acceptance FAILED (earlier pass, 2026-10-29)

Framework code `014e2cb5` (including `045d53e0`), isolated topic/worktree.
No main checkout, surface implementation, merge or PR was touched.

## Changes

- A CPU worker prepares/reclaims transform blocks and attachment tables. Guest
  append, seal, release and reset do not allocate, including when Rewind retains
  64 attachments (allocation-counted regression fixture). The prepared queue is
  bounded; exhaustion rejects optional publication and increments `storage_misses`.
  Pool storage is retained at its high-water mark, not returned after every tick.
  The 128 spare blocks reserve about 448 MiB of uninitialized host capacity;
  retained history can raise that high-water mark. This is a memory tradeoff,
  not a claim that CPU memory use decreased.
- Primitive provenance shares the following draw's transport group instead of
  forcing another submission. Grouped input is copied to aligned storage on replay.
- The render consumer leases immutable history blocks instead of copying all nine
  ticks every packet. Source tables, matching, departing geometry and texture-input
  restoration stay on render/native workers. Car Effects stays packet-owned.
- Cumulative `audited_pairs`, `failed_pairs`, `peak_unmatched` and `storage_misses`
  expose failures between polls. The moving audit checks cumulative deltas too.

## Final exclusive A/B measurement

Release with debug tools for telemetry, JOBS=4, hidden port 4822, 18x/full PGXP,
Car Effects and Rewind on. A disables the FPS feature; B enables it at the named
output rate. Each arm starts a fresh process, loads slot 9 and drives for three
seconds before its 60-second measurement. The same executable is used throughout.
The presented image ring is off; the existing 8x8 hash trace is on in every arm.
Distinct counts are **lower bounds**, not full-image or physical-scanout counts.
Even present completions are below 60/120, so the throughput gate fails regardless
of thumbnail collisions. The internal captured target is 7668 x 4320.

`tools/measure_interpolation_ab.py` checks Get-Process before admission/each arm,
creates `../perf.lock`, checks for other runtimes once per second, and releases
only its own processes and lock. All four final arms observed no other runtime.
An earlier attempt was rejected after another hidden phase-3 release runtime
started despite our lock; its overlap is preserved in the local rejected receipt.
No competing process was stopped.

| Output / arm | Distinct images/s (lower bound) | Present completions/s | Guest VBlanks/s | Starved samples | Max VBlank ms | Unmatched |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 60-A | 29.54 | 29.54 | 59.093 | 35,092 | 53.338 | 0 |
| 60-B | 51.34 | 52.92 | 57.768 | 98,416 | 57.629 | 0 |
| 120-A | 29.08 | 29.08 | 58.168 | 79,276 | 55.382 | 0 |
| 120-B | 94.02 | 95.93 | 58.129 | 86,642 | 54.573 | 0 |

**FAIL:** B adds 63,324 starved samples at 60 output and 7,366 at 120 compared with
its adjacent A. Guest cadence falls by 1.325 and 0.039 VBlanks/s respectively.
The allocation/transport changes do not establish performance parity. Earlier
exclusive runs also varied; no reliable throughput improvement is claimed.

**Separate baseline regression:** same-build FPS-off A itself reaches only
59.093 / 58.168 VBlanks/s with 35,092 / 79,276 starved samples. This is the
requested disabled-feature control, not an independent build of current r4/main.
Its complete guest profiles are in the evidence JSON. At 60 output, the main
named samples are host pacing sleep 29.4%, performance-counter calls 4.9%, native
raster enqueue 2.7%, PGXP loads 2.3%, ALU 1.8%, debug write checks 1.8%, and packet
record vector growth about 1.1%. At 120 the same groups dominate (native enqueue
3.0%, PGXP load/ALU 2.2/2.0%). These are sampled locations, not additive critical-
path timings; normal pacer sleep is not proof of the cause of a missed deadline.

B's profiles contain bounded projection lookup/transform append (about 0.1% each)
and common packet-buffer growth (1.3% at 60 / 1.2% at 120). The prior shared-pointer
history-array destruction hotspot is absent from the reported top addresses.
The zero-allocation fixture proves the collector's guest allocation removal;
profiles alone cannot prove that all scheduling/cache contention is eliminated.
The remaining timing deficit is unresolved, including the baseline deficit.

## Correctness, captures and verification

- 2,982 geometry packet pairs audited during the two B timing windows (1,525 /
  1,457); cumulative pair failures, peak unmatched and storage misses all zero.
  No dispatch or segment misses in any arm. Car Effects draw counts increase
  in both A and B, so an inactive effects path is not the explanation.
- After timing, frozen t=0 / 0.5 / 1 captures and repeated endpoints are
  7668 x 4320. Repeated endpoints are byte-identical. All five have 22,680 exact
  RGB(48,48,48) pixels in the lower 45%; midpoint adds zero clear-colour gaps.
  Visual inspection of the midpoint shows continuous road/walls in this sample.
  This narrow check does not establish every roof/visibility transition, an
  independent old-renderer comparison, or live cut/respawn acceptance.
- Five focused framework tests pass (stream, sampler, presenter, HD context,
  hidden GL packet replay). After the direct-lease change, stream and hidden GL
  replay pass again. The lifetime fixture releases producer history and source
  attachment before consumer matching. R4's 22 permitted CTests pass; the PGXP-off
  boot test is excluded. JOBS=4 regeneration leaves generated/ unchanged; build passes.
- All owned runtimes stopped, local mod state restored, perf.lock removed.

Reproduce: `python tools/measure_interpolation_ab.py --out analysis/interp-ab --capture`.
Raw final receipts/profiles/captures: `analysis/interp-exclusive-final/` (local).
Committed numerical proof: [A/B evidence](evidence/frame-interpolation-ab-20261029.json).
Rejected overlap: `analysis/interp-exclusive-after/`; preceding clean iteration:
`analysis/interp-exclusive-clean/`. Central Beads tracking was unavailable because
`F:/Software/beads/issues` does not exist in this environment.

**Do not integrate this as a completed phase 4.** Guest timing parity, zero
starvation and 60/120 distinct outputs remain unmet. Existing legacy redraw removal
still lacks the required full equivalence acceptance. No main merge or PR was made.

---

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

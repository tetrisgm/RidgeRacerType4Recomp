# Phase 3b: PGXP audit

## Outcome and scope

The paused race no longer alternates between two enhanced images. PGXP,
perspective texturing, perspective colour, depth, preserved projection, and seam
corrections stay enabled. Changes are generic framework precision/seam/shader
work. `gpu_gl_renderer.c` is touched, including a small local-coordinate sampling
change in `present_target_quad`; presentation queues, threading and frame
scheduling are not restructured.

The two remaining acceptance conditions are now closed: Car Effects freezes
while paused/static/rewinding, and all 842 >1-pixel projections have producer-
verified explanations. The one-pixel gate excludes only qualified exact
projections recovering hardware saturation or discarded fractions; unknown,
derived, near-plane/divide-overflow and screen-saturation differences still fail.
No PGXP feature is disabled. This continuation does not touch
`gpu_gl_renderer.c` or presentation plumbing.

## Matched before/after evidence

Hardware: NVIDIA RTX 4080 SUPER, OpenGL 3.3, driver 621.01. Hidden window,
`SDL_AUDIODRIVER=dummy`, debug port 4898, 9x internal rendering and 5120x2880
composed captures. Port 4798 was found occupied by another worktree, so the audit
moved to 4898. Only processes launched by the audit were terminated.

The comparison loads the same isolated paused race save (car visible, Car Effects
active). For the baseline, only the renderer and seam header were temporarily
restored from framework `cc5638e8`; current counters remained. Both files were
restored before the final build. The baseline and final native hashes and
normalized PGXP stream hashes match. No PGXP feature was disabled.

| Measurement | Previous renderer | Fixed renderer |
|---|---:|---:|
| Enhanced A/B differing pixels | 3,452 | 0 |
| High-resolution source A/B differing pixels | 264 | 0 |
| Presented image hashes | 2 | 1 |
| Native image hashes | 1 | 1 |
| Normalized PGXP stream hashes | 1 | 1 |

The final composed-image check contains four bursts of eight **consecutive**
presents across two savestate loads, plus eight before and eight after accepting
an earlier rewind snapshot. All 48 match. Four replay samples each contain 860
triangles; empty VBlanks are excluded. Car Effects recorded 565 additional draw
callbacks during the two-load replay check. The previous supervisor-provided
checkpoint differed at 3,379 output pixels; the 3,452 figure is the stronger
matched-checkpoint comparison from this continuation.

- Presented PNG SHA-256: `ee6d8cc053047c843cc6b479ee7fe64d998bb40616a0cc7fad1c6eefe5f6a2c0`
- Native PNG SHA-256: `7c786392ab8d0343f9bf24162f198855715e9f5579e2ce89d36cfeacf87668fb`
- Normalized vertex stream SHA-256: `bf09f4f4006a2807584995cecc48f2387a489373190b5a44837250a596ccaaf8`

Compact results are in [pgxp-audit-evidence.json](pgxp-audit-evidence.json).
Local captures and complete receipts are under `analysis/pgxp-audit/`; retail
saves and images are not committed.

## Causes fixed

1. Converting absolute 16.16 screen coordinates to float discarded different
   low bits in the two VRAM pages. Keep double precision until subtracting the
   draw-area origin, then emit local float vertices.
2. Seam indexing and barycentric expansion depended on absolute coordinate
   rounding. Normalize each seam domain and calculate barycentric displacements
   directly. Avoid an identity backdrop transform that needlessly adds and
   subtracts a large origin.
3. Hardware texture/colour interpolation retained tiny absolute-window-origin
   differences. A linked geometry stage carries triangle-local positions and
   reciprocal-weighted attributes; fragment interpolation is local and retains
   perspective correction. Raster positions use 1/256 of an output pixel.
4. Interpolating normalized presentation UVs changed fractional texel weights
   between pages. Resolve in local source pixels and add the integer origin only
   when fetching texels. This preserves bilinear and area filtering.

The previous agent's idempotent GTE configuration/cache-generation fix remains.
Neither precision fallback changes nor alternating depth domains explained this
static scene's A/B images: the accepted guest streams were already identical.

## Counters and lifecycle

Each drawn frame: 2,580 precision lookups, 58 integer fallbacks, 31 value
mismatches, zero truncation/tolerance rejects, and 2,220/2,220 valid depth-word
lookups. The normalized renderer stream contains 62,190 floats with zero changes.
A drawn-frame depth probe recorded 536 matching seam decisions, zero changed
seam decisions, two threshold clears, 625 failed depth samples and zero failures
within normalized epsilon 2^-20. Invalid w and depth input counters stay zero.
The probe is opt-in and synchronous; it must be off for performance measurements.

Disk loads and actual rewind acceptance preserve the final image/stream after
host precision provenance is invalidated and reconstructed. Existing GTE/PGXP
regressions additionally cover raw CPU restore, speculative/checkpoint rollback,
frozen extra-frame contexts and generation changes. The GL fixture uploads a
white texture and a U-gradient texture, then verifies actual perspective colour
and UV values, so a matching black image cannot pass as texture correctness.

## Car Effects pause and lifecycle closure

Root causes: R4's draw-frame counter continues during the pause menu, and
ambient-light smoothing/readback ran on every host presentation. The mod now
publishes the guest pause flag and speedometer with its race snapshot. A tested
clock advances particles, event emission and ambient sampling only for forward
guest frames with changed physical scene state, outside pause and rewind.
Sampler-owned event ages are excluded from scene comparison. Frozen frames
rebase the time origin, so resume never catches up elapsed pause time. Lighting
uses the copied speedometer rather than reading live guest RAM on the renderer.

A savestate callback requests a render-thread reset of disposable particles,
velocity/event history, ambient-readback history and RNG, retaining GL resources.
This also handles accepted rewind and makes a repeated forced burst deterministic.
The clock regression covers repeated presentations, paused draw frames, static
scenes, rewind, backward/large time jumps and forward resume.

| Forced-particle measurement | Before | After |
|---|---:|---:|
| Consecutive presented images in two-load replay | 32 | 32 |
| Distinct image hashes | 15 (one-load original test) | 1 |
| Changed pixels across the original paused burst | 566,784 | 0 |
| Simulation updates during paused replay | unguarded | 0 |
| Draw callbacks in final lifecycle replay | n/a | 612 |
| Verified forced backfires | 1 | 2 |

A further eight presents after accepting rewind and retriggering the burst match
the same image and vertex stream. Simulation stays frozen while rewind is open;
unpausing then advances it 60 times over 120 observed VBlanks. Dispatch misses
remain zero. Native images and normalized PGXP streams match their original
pre-continuation hashes above.

## Accuracy: all 842 differences classified

Counts below are mutually exclusive per accepted lookup, observed identically in
four drawn samples (2,580 lookups each). Tags are attached at the GTE producer,
travel only with the intact value-validated word, and are cleared by partial
writes or mismatched values. They never change geometry or acceptance decisions.

| Class | Count | Decision / reason |
|---|---:|---|
| Exact projection recovering IR1/IR2 saturation | 324 | PGXP is correct for enhanced geometry: integer IR clamps at -32768/+32767; the qualified unwrapped MAC retains the actual coordinate. Exclude from the integer-distance gate. |
| Exact unsaturated projection preserving MAC fractions and true division | 518 | PGXP is correct: integer IR/SZ discard 12 fractional bits and UNR approximates division. Their error can exceed one screen pixel. Exclude only with exact producer provenance. |
| SZ saturation alone / together with IR | 0 / 0 | Qualified exact projection is likewise correct when recovering depth above 65535; regression covers both, and counters remain explicit. |
| Near-plane / divide overflow | 0 | Not excluded. Exact projection rejects H >= 2*Z; ordinary IR projection must still agree. Any accepted >1-pixel case fails the audit. |
| SXY screen clamp without qualified exact recovery | 0 | Not excluded; any such difference fails. |
| Derived / unexplained | 0 / 0 | Not excluded; loss of provenance cannot silently grant an exception. |

Analytical regression: MAC1=4095, MAC3=151*4096, H=300, OFX=0 gives
integer IR1=0 and SXY.x=0; the exact position is approximately 1.98627.
The new actual-RTPS regression verifies that this reaches the fractional class.
Far-world fixtures verify `(40000,-35000,60000)` projects to `(360,-55)` with
OF=(160,120), H=300, while integer IR saturation gives different coordinates.
Another fixture covers combined IR/SZ saturation at Z=100000. Wrapped MACs,
lm sign clamps, stale words and partial writes retain their rejection tests.

The harness now requires all >1-pixel counts to equal the sum of the four
qualified exact classes, with zero unexplained/divide/screen cases. It also
requires zero simulation advances during a forced paused burst and verifies
that injecting the burst actually changes the image. Raw 842 remains visible;
it has not been clamped away or hidden.

### Matched-fixture compatibility

Execution-profile identity hashes include PGXP sources and headers, so adding
counters invalidates the original fixture's profile cookie even though guest
state, wire layouts and projection values are unchanged. Production validation
was retained. For this controlled comparison only, an isolated copy of slot 1
had its 4-byte profile cookie updated; all bytes after the 36-byte header are
unchanged (SHA-256 `6f7e6b04713bf16b19b3c9cc662c45992056e632ac4eb5ec13262173443b91af`).
The original profile cookie `586579a5` was reconstructed from committed source;
mod-memory layout `d94f80f6` was independently decoded from the save. The
original file was untouched. Local `fixture-migration.json` records the receipt.
Successful loads still pass every remaining loader check, and matching original
native/vertex hashes validate that the fixture represents the same scene.
This is local audit fixture preparation, not a save migration shipped to users.

Forced-burst PNG SHA-256: `2f206fe75de862f86dfbbdfa21330aea3a2e1fd25ab39c4be0a9498d8462860c`.

## Tests and reproduction

- The fresh hidden boot-to-race smoke reaches frame 8,721 with 3,676,439
  static dispatch hits and zero misses. Regeneration updates zero shards.
- `r4_effects_clock` builds and passes through CTest.
- Five CPU suites pass: PGXP, word-shadow, seam snap, seam batch, GTE register
  access. The new seam test requires bit-identical decisions across translations.
- Hidden real-GL regression: 2,000 seeded triangles of each of four kinds at
  three VRAM origins, scales 1x/4x/9x and a separate 18x run. Zero translated
  pixel differences. Fractional presentation resolve, analytical perspective
  colour/UV, invalid w/depth rejection, uploads and shader-stage linkage pass.
- Calibrated near-depth samples: 2,485 / 39,340 / 198,765 / 794,430 at
  1x/4x/9x/18x. All count as near in the close trial, none in the far trial.
- Full render-thread fixture: 80 frames, thread off/on at 1x/4x plus dynamic
  resolution at 4x. All readback/native/high-resolution/wide digests match;
  ring wrap and ten dynamic-resolution steps pass.

Start the runtime hidden with dummy audio and `PSX_PRESENT_SHOT_BURST=8`. Load
an isolated paused race slot, then run:

```powershell
python tools/pgxp_static_audit.py analysis/pgxp-audit/recheck --port 4898 --samples 2 --expected-burst 8 --replay-slot 1 --replays 2 --require-effects --particle-car 0
```

The harness checks the real GL interpolation stage, requires nonempty drawn
streams, compares native images too, and refuses incomplete burst captures.
Use a new output directory. Allow load notifications to expire; the default
settling delay is six seconds. For rewind testing, enable `PSX_REWIND=1`, open
F8 through `host_key`, and navigate/accept using `set_input` with `layer:"host"`.
Clear the input afterwards. The local `run_lifecycle.py` records that sequence.

Framework diagnostic details and the CMake test/compiler option are documented
in [psxrecomp/docs/PGXP_AUDIT.md](../psxrecomp/docs/PGXP_AUDIT.md). The central Beads
path `F:/Software/beads/issues` is unavailable on this host; no local database was
created. No changes were made in the owner's main checkout or its build folder.

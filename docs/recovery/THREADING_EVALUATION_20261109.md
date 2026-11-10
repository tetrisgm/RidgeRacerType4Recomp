# Threading and performance history evaluation — 2026-11-09

## Conclusion

The four restored renderer optimizations are only one part of the work. The
older fork also implemented private draw workers, native raster ownership,
independent presentation packets, audio transport and background snapshot work.
Several have useful correctness evidence; they are not all failed experiments.
They also are not interchangeable patches for the current engine.

Keep the current upstream-based native execution, custom menu and controller
fixes. Reuse the validated mechanisms selectively. Do not reinstate the complete
October renderer: its later fixes demonstrate unresolved ownership/visual risks,
and its best cadence receipt does not establish full visual equivalence.

This evaluation inspected implementation snapshots and their historical receipts,
including work by the owner and collaborators, rather than filtering by author
or cherry-pick date. Historical measurements below are not fresh benchmarks.
The newly run Mac tests are identified separately. This is not an independent
revalidation of every archived build, course or Windows measurement.

## Implementations and decisions

| Work | Evidence and current relationship | Decision |
|---|---|---|
| Audited idle-loop elision (`dc7e3cc3` family) | Current `game.toml` enables store-counter skipping only at audited `0x8008B4D0` / `0x8008B518`. Historical 3,600-frame equality and roughly one-third lower emulation CPU are recorded there. This is real work already retained, not a missing thread. | Keep. Reproduce CPU savings only if profiling implicates guest execution; do not generalize skipping to unaudited loops. |
| PGXP FIFO (`227cb935` and equivalent lineage) | Current `pgxp.cpp` retains SXYP shadow shifting and SXY2 mirroring. | Keep; ancestry alone is not a reliable missing-feature test. |
| PGXP emitter reset specialization (`519a9836`, preserved generated working patch) | Old emitter unconditionally reset precision for AND/XOR/SLT families. Current `pgxp_hook_emitter.cpp` passes operands and preserves valid coordinate/scalar provenance through these operations. | Do not replay the generated patch or old classification. Any new specialization must preserve current semantics and come from the emitter. This is a concrete supersession, not merely missing qualification. |
| PGXP delta checkpoints (`8c6b657a`, game `2c3d3430`) | Moving 600-frame probes recorded about 610 kB/147 dirty pages per delta, 34–38 microseconds delta copy versus earlier 1.2 ms synchronous checkpoint; seed 5–8 ms. No worker image or whole-game gain was measured. | Useful technique for consumers needing checkpoints. Current ordinary native-cadence rendering does not need extra guest-draw checkpoints; do not add them to exploit this result. |
| Private draw contexts/pool (`9f34cf89`, identity fix `9e0f325e`) | Separate CPU/GTE/PGXP state and generated draw entry points. One moving checkpoint matched 19,669 commands and all 7,668×4,320 pixels, including four parallel contexts. Depth/seam history fixes were essential. Later packet cutover `2e39cbe6` removed the old production draw pool. | Preserve the tests and ownership design. Not the preferred restoration path: re-executing guest draws and publishing per-job effects metadata add work, and the narrow identity receipt is not a sustained throughput/all-course gate. |
| Owner call gate (`84d6ca22`) | Code marshals a worker request to its owner, protects live payload lifetime, and waits beyond the requested timeout once servicing has begun. New Mac fixture passes ordering, refusal, timeout, exceptions and close. | Reusable when a worker truly needs an owner-only service. It replaces unsafe access/polling; it cannot make the serialized service parallel or guarantee bounded callback duration. |
| Native raster thread / SPSC (`066006db`, `35a8f1a0`) | Original GP0 capture precedes presentation transforms. Separate native authority supports ordered reads, partial uploads and cache-preserving snapshots. Later fixed 16,384-record ring and 4 MiB arena remove recurring submissions/allocations. Historical oracle and blocked-consumer fixtures exist. Current runtime has no equivalent separate native-raster pipeline wired into main. | High-value architectural work if current guest waits are caused by renderer-owned native results. Port as an ownership unit with parser/snapshot tests, not just the queue class. Native completion is still a dependency; CPU-only does not mean free. |
| Complete packet mailbox / permanent GL owner (`1d14aef6`, `84d6ca22`) | Latest-wins consumption and epoch-controlled requests remove dependence on every presentation frame completing. Packets need independent texture inputs and retained enhanced surfaces. Subsequent `734794d5` fixes excessive tile uploads and interpolation across dropped packets; `91b54cd2` fixes bloom surface dimensions. | Do not switch the current incremental render ring to dropping records. A useful future design must preserve skipped upload/copy effects, surface history, HD provenance, controls and resets. These are correctness dependencies, not optional polish. |
| Allocation/guest-hotpath work | `84d6ca22:docs/internal/GUEST_HOTPATH.md` records SPSC 55.87, guards/direct fill 58.10, pooling 60.12, final 59.68 VBlanks/s. One initial baseline was contaminated; final maximum interval 49.72 ms still missed the gate. | Preserve individual allocation/ownership tests. These sequential historical samples support investigation, not additive gains or a proven 4K Mac benefit. Much of the optimized allocation belonged to the removed packet pipeline. |
| Lock-free audio transport (`84d6ca22`) | Callback alone owns resampler; producer publishes PCM through SPSC; reset cuts old timeline without discarding new samples. Current main still uses shared bridge push/pull and device synchronization. New Mac exact-waveform and concurrent-wrap fixture passes. | Strongest small threading candidate for a controlled port. Preserve current volume/menu/pause/load semantics; measure callback contention first. Does not repair a guest running slower than audio consumption. Historical adaptive target/clock policy is a separate behavior change, not required by SPSC. |
| Rewind worker / delta publication (`6cb8b57d`) | One outstanding job; retained wake avoids lost notifications; teardown waits for completion. New Mac fixture passes. Historical implementation couples worker to native checkpoints, pooled deltas and hold-to-rewind. Current rewind uses capture-ring snapshots and retains XA-density fix. | Port only after measuring capture spikes. Passing worker fixture does not prove immutable snapshot input or correct load/discard ordering in today's serializer. Retain current XA-density/HD restore fixes. |
| Disc prefetch (`60b2c85f`, `81862e0a`) | Independent reader and 64-slot cache, 24-sector lookahead; mod overrides/patches remain after base read. Cache miss still reads synchronously. Global map lookup returns an unleased pointer: concurrent close must remain excluded by caller lifecycle. Historical BIN test demonstrates hits, not speed. | Candidate for measured I/O stalls. Require mount/close, CHD, mapped CDDA and patch tests. Local cached BIN racing may gain little; no reason to prioritize ahead of measured render cost. |
| Render priority (`c1523113`) | Windows-only priority request; no Mac effect. Historical interpolation priority experiment actually worsened results and was reverted. | Not a Mac performance fix; Windows benefit requires owner-side measurements. |
| New explicit geometry snapshots (`c7ebe109` through `1b84d4fd`) | A separate development lineage uses title-provided object/lifetime/topology metadata, immutable textures and clipped geometry; its documented 3x pilot is Ape, not R4. Missing/late samples retain native deadline. | More defensible interpolation direction than nearest-position matching, but requires an R4 producer and R4 qualification. Do not describe its pilot as recovered R4 performance or assume it is in the pinned/upstream master. |

## Resolve the apparently contradictory historical results

[Threaded draw identity](../THREADED_DRAW_IDENTITY.md) is strong evidence for one
checkpoint. [Interpolation status](../FRAME_INTERPOLATION_STATUS.md) records both
real visual regressions and later narrower improvements: zero unmatched counts
could coexist with missing walls/roof; frozen endpoint replay was not independent
stock-renderer equivalence. Wait-parity later improved within large baseline
variance, while distinct output remained below its target.

[October 30 receipt](../evidence/final-release-20261030.json) reports 59.994
VBlanks and completed presentations per second at 18x for 121 seconds on Windows.
It explicitly counts swap completions, not unique images or physical scanout.
That successful run must be retained as evidence, but cannot close every earlier
visual/cadence target or be compared directly with today's moving 4K Mac test.
MStan's game commit `d72d212c` deliberately returned to upstream/native spans and
removed the fork packet path; this was an explicit recovery decision, not proof
that all the removed engineering was worthless.

The November 5 slowdown record adds a different clue: zero render-ring
backpressure in its observed route alongside expensive canonical GPU work.
Its GPU and VBlank samples were not synchronized, so it does not prove that
threading or a particular pass caused every slow interval. Max Detail's heap
budget controls guest capacity, not host GPU cost; the 8 MiB fix prevents heap
corruption and does not itself promise faster rendering.

## What to do next

1. Measure the current moving 4K route with synchronized guest CPU, ring/drain,
   rewind, render submission and GPU completion timing. Preserve full PGXP,
   Max Detail and owner settings. Separate GPU work from host driver waits;
   measure overhead of diagnostics. Existing 47.306 VBlanks/s is below target.
2. If audio synchronization or snapshot work consumes the critical path, trial
   the isolated SPSC transport or immutable snapshot publication respectively,
   each against the current menu/save/load/controller baseline.
3. If native-result waits behind GL dominate, evaluate native authority and
   packet ownership together. If canonical raster dominates, prioritize its
   redundant passes/uploads/surface work instead of adding threads.
4. Treat higher-rate interpolation as a separate feature with geometry and
   distinct-image gates. It cannot be credited with fixing slow native gameplay.

## Fresh validation and scope

On this Mac, extracted unmodified historical components passed: owner call gate
at `84d6ca22`, audio transport at `84d6ca22`, rewind worker at `6cb8b57d`.
Commands/results are recorded in `THREADING_COMPONENT_TESTS_20261109.json`;
extracted files and executables stay local in
`analysis/threading-evaluation-20261109/`. These tests do not invoke the game,
disable PGXP or modify the archived repositories. No experimental runtime
change or new performance claim accompanies this evaluation.

Current Release build, 29/29 CTests, custom-menu checks and the native-overlay
boot-to-race smoke pass. Smoke records 7,180,137 static hits and zero dispatch
misses. See the local `menu-smoke/native-overlay-check.json` receipt.

References: [previous restoration](MSTAN_RESTORATION_20261109.md),
[original recovery assessment](TECHNICAL_ASSESSMENT.md),
[November slowdown evidence](PERFORMANCE_HANDOFF_20261105.md),
[architecture design](../FRAME_ARCHITECTURE.md),
[historical pipeline map](../FRAME_PIPELINE.md). Architecture documents describe
their recorded pins, not necessarily today's executable.

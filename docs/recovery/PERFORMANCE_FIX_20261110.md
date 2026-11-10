# Fixed 4K render stalls — 2026-11-10

The M4 now sustains full game speed in the qualified moving race: **59.936
VBlanks/s for 120 seconds**, fixed 9x / 2,160 internal lines, 21:9, full PGXP,
Max Detail, HD HUD/effects and rewind retained. Every five-second presentation
sample is 29.89–30.05 FPS, the game's native rendered cadence. This does not
claim 60 unique game images/s or physical 4K display scanout.

## What changed

Engine `9fbb6ba8` retains MStan's native execution and the four restored renderer
optimizations, plus three focused changes:

- `7b93a5c8`: adapt preserved `a1af76d0` to copy authoritative native VRAM on
  the render owner, in command order. Rewind no longer migrates the GL context
  just for that copy. A drain still guarantees completion before the caller
  reads it; no asynchronous incomplete snapshot is exposed.
- `c7b69a3d`: continuous forensic framebuffer capture is opt-in when threaded
  rendering is active. Explicit `PSX_DISPLAY_RING=1` still enables it; TCP
  telemetry and one-shot screenshots remain. This restores the intent of
  earlier `40003728`. Windows production builds already excluded this code.
- `7f71774f`: queued PGXP depth resets use scissored hardware clears instead
  of shading two screen-sized triangles. Ordering, per-surface scissor, far
  depth, color/stencil preservation, high-resolution tiles and wide replay
  remain. The old two-pass polygon depth tolerance is unchanged.

Custom menu/UI `ef8150f`, Select/Back, controls/settings and Steam Controller
code are unchanged. PGXP is never disabled, and no resolution/detail/default
player setting is reduced. No packet-mailbox architecture is imported.

## Measurement

Fresh isolated processes load the same eight-car grid save and run the same
closed-loop driver. Driver VSync and continuous display capture are disabled
in **both** arms; normal game pacing stays on. No builds or component tests
ran during these accepted windows. Other user applications remained active.

| Trial | Earlier implementation | Candidate |
|---|---:|---:|
| Snapshot copy, two alternating 30-second arms each | 46.293 / 49.885 VBlanks/s | 51.223 / 51.223 |
| Depth clear, separate 30-second pair, snapshot fix in both | 50.938 | 59.973 |
| Final moving race, 120 seconds | — | 59.936 |

Snapshot context acquisitions drop from 128/137 to 12 per run; measured
acquisition time drops from 3.60–3.78 seconds to 0.10–0.18 seconds. In the
short depth-clear comparison, render-frame backpressure drops from 13,938 ms
to 29 ms. These counters cover each process lifetime, including warmup, and
are not additive CPU/GPU cost measurements. The long run records 138 ms total
backpressure and zero dispatch/segment misses.

The original profile located continuous `debug_server_record_frame` GPU
readbacks, frequent `vram_transfer_out` context transfers and render-side swap
waiting. Swap waits alone do not identify which GPU operation caused them;
the controlled depth-clear experiment and exact attachment comparison provide
the causal evidence. Final late-window scene GPU average is 8.44 ms. Earlier
November 9 Mac reports used continuous debug capture and therefore describe
that instrumented build, not production overhead. They remain historical
receipts, not the acceptance baseline for this fix.

Raw runs and executable hashes are in [the receipt](PERFORMANCE_FIX_20261110.json).
Measured binaries were built during qualification before the final commits;
the receipt records their exact hashes. Delivery retains that runtime code
with completed fixture registration/documentation.
Local scripts, samples, screenshots and logs are under
`analysis/performance-fix-20261110/`. Initial sampled diagnostics are excluded
from gain claims, including one that overlapped a rebuild.

## Correctness and delivery

The new real-GL fixture compares full depth, color and stencil attachments
against the former triangle implementation. It verifies actual allocated scales
1x/4x/9x, both framebuffer halves, narrow drawing areas, native-wide margins
and high-resolution tiles: **72 full-surface and 86 tiled checks pass**. Early
fixture setup mistakes (requested versus active scale, unallocated wide target)
were corrected before accepting these receipts. Both modes are registered in
the framework test suite.

Six render-thread GL modes pass, including native snapshot ordering, wrapping,
immutable returned data and no context acquisition. HD replacement full/window
fixtures and native-baseline hashes remain unchanged. Release and all 29 game
CTests pass. Supported regeneration reuses all 7,459 audited native variants
without generated source changes. Final menu/save/load/persistence and the
40-step boot-to-race smoke pass (7,181,192 static overlay hits, zero dispatch
misses), recorded under `menu-smoke`.

Portable delivery: `dist/windows/2026-11-10-4k-performance-01/`, cross-built on
the Mac with debug tools excluded. Its BUILD.json and execution sidecar identify
the source and executable; SHA256SUMS covers staged content. Windows hardware,
physical Steam Controller, audible output and all-course/netplay coverage remain
owner-side qualification. NAS and STEAMBOX were not accessed.

# Mac performance qualification — 2026-11-09

This report predates restoration of four parked performance candidates. Its
baseline comparison does not establish that all MStan optimizations were
present. See the [subsequent restoration](../recovery/MSTAN_RESTORATION_20261109.md).

The custom-menu restoration does not show a performance regression against
the preserved pre-restoration baseline. **1440p sustains full game speed on
this Mac in the tested moving and eight-car-start routes. 4K does not.**
No game code, settings defaults or owner settings were changed by this audit.

## Results

Apple M4 / 16 GiB, OpenGL, 21:9, Max Detail and expanded 8 MB RAM, PGXP on,
HD HUD/effects retained, fixed resolution, frame generation and present thread
off. Full speed is approximately 59.94 VBlanks/s; observed race presentation
at full speed is approximately 29.97 frames/s. These are distinct measurements.

| Scenario | Build / resolution | VBlanks/s | Presentation result |
|---|---|---:|---|
| Stationary, two 20 s runs | Baseline / 4K | 59.968 | 29.976 fps mean |
| Stationary, two 20 s runs | Restored / 4K | 59.981 | 29.976 fps mean |
| Moving, 60 s | Baseline / 4K | 54.79 | lowest 5 s window 20.34 fps |
| Moving, 60 s | Restored / 4K | 54.82 | lowest 5 s window 19.08 fps |
| Moving, 60 s | Restored / 1080p preset | 59.94 | all windows 29.93–29.99 fps |
| Moving, 60 s | Restored / 1440p | 59.94 | all windows 29.92–29.99 fps |
| Eight-car start, 30 s | Restored / 1440p | 59.98 | all windows 29.96–29.98 fps |
| Eight-car start, 30 s | Restored / 4K | 49.98 | lowest 5 s window 18.20 fps |

The 1080p comparison uses integer 5x rendering (1,200 actual internal lines);
1440p is 6x and 4K is 9x/2,160 lines. Effective scales and dynamic-resolution
off were checked from the runtime, not inferred from menu labels.

The 4K grid run reaches a worst five-second-window p95 present interval of
62.763 ms. Its sampled mean scene GPU time is 20.94 ms versus 11.37 ms at
1440p. The moving 4K restored run also records 10.32 seconds of render-queue
backpressure over its process lifetime. These support rendering pressure as
a lead; they do not isolate a single function or prove an optimization.

## Method and evidence

- Exact revisions, executable hashes and all 14 measurements are in the
  [machine-readable receipt](MAC_PERFORMANCE_20261109.json).
- Local scripts and raw receipts: `analysis/mac-performance-20261109/`.
  `bench.py` runs the alternating/reverse-order stationary comparisons;
  `moving.py` and `moving-low.py` drive the course using the existing autopilot.
  `grid.py` captures a fresh rank-8 start during the 40-step smoke route;
  `grid-moving.py` measures that start at 1440p and 4K.
- Both comparison builds use isolated copies of the same save, current mod
  assets, settings and input policy. Moving runs share initial conditions and
  the same closed-loop driver; wall-time-driven inputs do not produce perfectly
  identical guest-frame trajectories. Camera positions and screenshots confirm
  movement through the course. The initial stationary scene alone was inadequate.
- Eight stationary runs provide two samples per build/resolution at the 1080p
  preset and 4K. Moving runs measure 60 seconds after warm-up; grid runs measure
  30 seconds. Screenshots occur outside timed windows. Presentation telemetry
  measures host API completion, not physical display scanout.
- All measured runs have zero dispatch/segment misses, PGXP enabled and dynamic
  resolution disabled. Mac Release, 29/29 CTests and the complete 40-step smoke
  sequence pass again, with zero dispatch misses through frame 8,490.

## Limits and next work

Media analysis and Spotlight consumed substantial CPU throughout the session;
we did not stop unrelated work. These results describe the Mac under that
background load. The initial hidden-window VSync runs blocked in
`Cocoa_GL_SwapWindow` and aborted via the starvation watchdog. Diagnostics
identified the wait; timed tests used process-local `PSX_VSYNC=0` while keeping
the game's normal speed limiter. `PSX_DYNRES=0` ensured a fixed rendering cost.
The owner's VSync, resolution, save files and other settings were untouched.

1440p is the supported recommendation from these tests. It is not a silent
replacement for the owner's 4K choice. The existing 4K slowdown remains open;
claiming universally smooth 4K or complete project performance would be wrong.
Visible-window VSync pacing, real audio, all courses, the owner's specific
rank-8 crash route, physical controllers and Windows still need their own
qualification. No NAS or STEAMBOX access occurred.

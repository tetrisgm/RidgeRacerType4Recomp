# Phase 5: native per-frame rewind

Implementation on `feat/frame-arch-5-rewind`, based on native-authority
`4f893c1a`. Framework code is title-independent. The supervisor integrates;
this topic does not merge phases 2-4 or change `gpu_gl_renderer.c`.

## State and lifetime

Every guest frame requests capture at the next resumable basic-block edge.
CPU registers, RAM, scratchpad, device state, clocks, instruction-cache tags,
executable-RAM bitmap and mod memory stream directly into a reverse-page journal.
Changed 4 KiB pages retain their predecessor bytes; unchanged pages are shared
through the current image. Content comparison covers DMA, HLE and direct-pointer
writes without introducing write-barrier overhead or omitting untracked writes.
Variable-length device FIFOs retain their previous lengths across rewind.

GPU registers and partial-command state are captured on the guest thread. An
ordered CPU-native checkpoint captures VRAM and stale-but-valid texture/palette
cache entries at that same command-stream position. Guest execution can continue
while it completes. The snapshot stays unpublished until both sections have
been patched into its journal. Polling, patching and publication costs are
included in the capture counter. Destruction/discard waits for CPU completion
before freeing destinations. No snapshot reads GL or enhanced render targets.

The legacy 50-snapshot preference now retains 750 every-frame edges, about
12.5 seconds at 60 Hz; the old sparse interval preference no longer reduces
capture cadence. The ring evicts oldest edges and discards the future as the
user rewinds. Storage in the measured race was roughly 95 MiB, rather than
750 full machine copies. A disk-state load or disc change discards history.

## Interaction

Hold F8, the configured host shortcut, or Y in R4 Modern controls. Each restored
native scanout is presented at a nominal 60 Hz host cadence. Release re-enters
the scheduler at the restored PC. No guest instructions execute between reverse
steps. At the oldest retained frame, holding keeps that frame displayed.
Audio is paused during reverse playback; old host PCM and resampler history are
discarded, with a fade into resumed output. PGXP configuration stays enabled.
Enhanced output is disposable: normal rendering reseeds after release.

There is no filmstrip, selection cursor or accept-to-load action in the default
path. The old overlay ABI remains inert for compatibility. Netplay and the
existing title block (R4 split screen) still refuse rewind. No launcher source
change was needed: input and the former filmstrip were owned by the runtime.

## Measured evidence

Windows, AMD Ryzen 7 9800X3D, GCC 16.2 Release with debug tools, worktree-local
build, hidden OpenGL window, `SDL_AUDIODRIVER=dummy`, port 4798. Rewind, 60 FPS,
Car Effects, geometry/texturing/CPU-mode/culling/depth/colour/seam PGXP defaults
were enabled. No PGXP-off reference was run.

| Race capture window | Captures | Mean host capture cost |
|---|---:|---:|
| TCP hold demo | 605 | 0.381678 ms |
| Additional window 1 | 601 | 0.406631 ms |
| Additional window 2 | 587 | 0.407000 ms |
| Additional window 3 | 585 | 0.427349 ms |
| Final rebuilt TCP demo | 601 | 0.467880 ms |

These are measured emulation-thread costs for begin, readiness polling, delta
patching and publication. Native raster work is asynchronous on its CPU worker.
They are averages, not a hard real-time maximum: the longer validation run
observed a 26.18 ms capture outlier, including allocation/host scheduling.
This does not assert phase-2 guest pacing or phase-4 enhanced interpolation
acceptance. Debug-disabled performance was not separately measured.

The TCP demo verified 122 consecutive restored full serialized states byte for
byte, then resumed capture. A separate host-input Y hold/release test verified
another 52 states (174 total), with no restore mismatch or dispatch/segment
miss. The final rebuilt demo independently verified another 124 restores.
Tests check the live restored machine, not merely the stored delta bytes.

Component checks: reverse delta eviction, variable-length tail, fragmented
streaming, abort rollback, branch-after-release, delayed native completion,
muted reverse presentation, safe-edge gating, netplay/title gates, raw/streamed
serializer identity and atomic load rejection. Native raster fixtures also
check asynchronous queue-position identity and completion after worker teardown,
including texture-cache state and the existing pinned native pixel fixtures.

R4 CTest: 31/31 passed. `r4_pgxp_boot` is excluded because that legacy test
explicitly runs PGXP off. Live all-on debug checks replace that portion here.
Boot-to-race smoke: zero dispatch and segment misses at every scripted step.

## Reproduce

Build in an isolated checkout with `JOBS=4`, Release and `PSX_DEBUG_TOOLS=ON`.
Start with `SDL_AUDIODRIVER=dummy`, `--hidden-window --no-launcher
--debug-port 4798 --memcard-dir build/rewind-saves`. Supply the worktree's game
config/disc and its own overlay compiler as in `tools/run_r4.sh`.

```
python tools/smoke.py build/rewind-smoke --port 4798
python tools/rewind_demo.py --port 4798 --out build/rewind-proof.json
```

`rewind_status` returns frame, captures, restores, failures, retained bytes,
cumulative capture time, maximum, and serializer phase totals. The debug-only
`hold:1/0` drives the same reverse loop; `verify:1` reserializes each restored
machine and compares every byte against its snapshot. Verification runs only
while rewinding and is excluded from capture measurements. For the real mapped
button path use `set_input` with `layer:"host", buttons:"0xEFFF"` (Y), then
`buttons:"0xFFFF"` to release. `screenshot` selects native scanout during rewind,
since the ordinary enhanced wide surface then belongs to an abandoned timeline.

Local proof artifacts are under `build/rewind-*.json` and
`build/rewind-final-smoke/`. Game images and saved states remain untracked.

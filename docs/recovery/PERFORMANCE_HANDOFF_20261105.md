# R4 race slowdown handoff — 2026-11-05

## Owner requirement and current status

The owner hears music and effects slow with the game in crowded races, especially
near the start. Preserve the chosen 4K internal resolution, Max Detail, PGXP,
and presentation-thread-off setting. Do not trade visual quality for speed.
The slowdown is reproduced but **not fixed**. The separate race hang has an
identified memory-corruption cause and a built Max Detail manifest fix.

Work in `C:/Users/Shokunin/dev/r4-mstan-validation`. Read `CLAUDE.md` and
`docs/recovery/README.md`. The original root checkout is obsolete and dirty.
Do not alter the owner's `build/settings.toml`, `saves/`, Steam entry or desktop
shortcut. Do not push or merge. `desktop-r4-archive/` is an unrelated untracked
archive; preserve it. The diagnostic profile, state and telemetry are ignored
under `analysis/temporal-check/`.

## Crash fix already built

The owner's unresponsive race had a fail-fast dispatch at `0x800600D0`, frame
3030. The generated dispatch already contains that address. A guest polygon
writer ran 1272 bytes beyond buffer 1's retail heap end (`0x800F2AA0`), wrote
to `0x800F2F98`, and ultimately overwrote code at `0x800600D3`. This is the
root of that dispatch miss. `user-hang-20261105-*` holds the crash report and
freeze trace. Max Detail now declares a requirement on the bundled 8 MB RAM
feature in `mods/preloaded/packages/r4.enhancement.max-detail/1.0.0/manifest.toml`.
The existing plugin relocates both primitive heaps to 1 MiB regions when 8 MB
is active. Resolver log confirms implicit activation, and a live race heap
pointer of `0x80304D8C` confirms use. CMake build and CTest 28/28 pass. The
clean-boot smoke reached a race with zero dispatch/segment misses. The desktop
build's staged catalog has the requirement. Native card saves are unchanged;
old 2 MB savestates cannot load under 8 MB.

## Valid performance evidence

The `full-race-8mb-complete.json` automated run started at an isolated 8 MB
grid state, used the owner's 4K/Max Detail/PGXP profile and traversed 520 of
523 Helter Skelter segments in 137 seconds. Its 137 one-second samples had
game VBlank counts min 28, median 56, mean 53.1; 61 were below 55 and 23
below 45. Render queue backpressure was zero and dispatch/segment misses were
zero. The worst extended stretch was around track segments 214–150. This was
a hidden, muted run, so GPU timings may be affected by low power state.

`final-visible-4k.json` is a separate visible, muted, normally paced 35-second
run from the same isolated grid with debug turbo explicitly off. It advanced
127 segments. Game VBlanks/s min 24, median 60, mean 57.8; 6 of 35 one-second
samples were below 55. Queue backpressure was zero. During its slow section,
the RTX 4080 SUPER was in P0 at 2775 MHz SM/11501 MHz memory and the
`frame_perf` recent ring (256 native-wide frames, 2334 primitives/frame)
reported 32.37 ms average scene GPU time, 95.56 ms maximum, 29.79 ms canonical
scene and 2.58 ms mirror average. That is near or above a 30 Hz visual-frame
budget and shows GPU workload is a plausible immediate bottleneck even when
the card is fully clocked. The debugger's `frame_perf` ring and one-second
VBlank samples were not synchronized per frame; do not claim a proven causal
mapping for a particular dip from these aggregates alone.

The Max Detail trace during a 4K race shows the governor at level 30, its
maximum, permitting up to 32 ahead sections and a 1 MiB primitive heap only
5–8% used.
It added roughly 900–1700 PVS blocks per 60 game frames and retained tens of
thousands of cone candidates. The expanded heap removes retail pressure, so
the governor has more room to climb; it uses guest cycle/heap budget, not
measured host GPU cost. This may explain excess work, but no visually lossless
replacement has been tested.

Several diagnostic comparisons are **invalid**: the first `ab-no-sections`
and `ab-1080p` processes failed to bind port 4798, so their scripts sampled a
different live process. A later clean hidden no-sections run bound correctly,
but had much higher per-primitive GPU cost despite fewer primitives and ran
with the GPU in low-power P8; it is not evidence that disabling sections
helps or hurts the visible game. Do not use the 1080p result as a proposed
fix. Audio underrun counters under `SDL_AUDIODRIVER=dummy` do not represent
audible playback. The owner previously heard genuine slowdowns in normal audio.

## Next investigation

1. Capture a **visible, normally paced, normal-audio** 4K race with synchronized
   frame/scene GPU timings, host CPU samples, VBlanks, game ticks, audio fill,
   GPU clocks and a track position. Keep the owner's saves isolated. Do not
   leave debug turbo enabled after a savestate load.
2. Profile the canonical OpenGL scene workload in crowded sections. Determine
   whether primitives, texture filtering, high-resolution VRAM clears/copies,
   repeated readbacks, or GPU/CPU synchronization dominate. `frame_perf`,
   `render_thread`, `video_info` and `display_timing` are available on the
   debug port. Instrument only if the existing aggregates cannot identify
   the hot operation. Preserve pixel/visual parity in any optimization.
3. Use same-build, same-state A/B runs with **one** game process and verify the
   debug port bound to that PID before recording. Pair alternated baselines
   under comparable host/GPU load. Diagnostic ablations can isolate costs,
   but do not ship reduced resolution, detail, reflections or draw distance.
4. Fix the actual hot path, build, run CTest, rerun a full lap, compare the
   exact slow sections and visual output, then have the owner test the build.

The ignored `full_race_profile.py` is the automated driver and sample collector.
It now explicitly clears TCP turbo after loading slot 5. It uses the host
virtual pad because Modern trigger mapping ignores guest-button injection.
`build/r4-runtime.exe` and the desktop shortcut still use the current staged
mod catalog; no performance fix has been made.

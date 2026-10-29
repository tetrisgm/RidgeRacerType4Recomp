# R4 frame architecture: target design (2026-10-28)

Companion to [FRAME_PIPELINE.md](FRAME_PIPELINE.md), which maps today's
pipeline. This document is the replacement design. It is framework work
(psxrecomp), generic to every title, faithful by default.

## The problem in one sentence

Today one pipeline does two different jobs, **computing the exact PS1 result**
and **drawing a pretty 8K picture**, and the two are welded together. The
guest's clock waits on the GPU (rt_drain, rt_acquire and context switching
took 44% of samples), and the picture pays PS1 costs it doesn't need (a
1.3 GB scaled copy of all of VRAM, transactional backup and restore for
extra frames, canonical plus wide double drawing, separate depth passes).

## Principle: two worlds, one-way flow

```
 GUEST WORLD (exact, native, deterministic)        PRESENTATION WORLD (enhanced, disposable)
 ───────────────────────────────────────────        ─────────────────────────────────────────
 emulation thread: recompiled MIPS, GTE, PGXP        render thread (only GL context owner):
   CPU-mode, GP0/DMA parser, SPU, devices              frame packet -> wide target (8K, PGXP,
          │                                             depth in one pass) -> postfx -> swap
          │ GP0 stream (native)                               ▲
          ▼                                                   │ immutable frame packets
 native raster thread: exact 1 MiB native VRAM ──────────────┘ (draw list + PGXP vertices +
   (software, the ONLY VRAM authority)                          uploads + display + race state)
          ▲
          │ reads: GPUREAD/StoreImage, savestate, rewind, HD texture keys, netplay
```

1. **Guest world never waits on the GPU.** The exact native VRAM lives in CPU
   memory, kept by a native software rasterizer on its own thread (the HD
   native-authority raster that exists today, made universal). GPUREAD,
   StoreImage DMA, savestates, Rewind, netplay and HD texture keys all read
   it. The only wait is on the native raster thread catching up, which is
   CPU-only, microseconds, and never behind GL work. The GPU-to-native
   "pack" path and every `glReadPixels` on the guest path are deleted.
2. **Presentation is a one-way consumer.** Each guest frame emits one
   immutable **frame packet**: primitives with PGXP-precise vertices and
   state, native upload and copy rectangles, display config, and title
   metadata (R4 camera and car state for Car Effects). The render thread is
   the only GL context owner, forever. `GL_RT_SYNC`, context acquisition and
   ownership publication are removed from every runtime path (diagnostics
   get queued requests with async results).
3. **Guest time is sacred; presentation degrades.** Presentation uses a
   mailbox: the newest finished image wins, and old ones are dropped. If the
   GPU falls behind, we show fewer distinct images, and the guest clock,
   audio and input never slow down. Backpressure from the render thread to
   emulation is bounded to about 2 frames of packets and never blocks a VBlank.
4. **Draw what's shown, at the size shown.** No scaled copy of all of VRAM.
   - The display is drawn **directly into the wide output target** at the
     chosen resolution (one pass, not canonical plus wide mirror replay).
   - Offscreen render targets that the game later samples as textures
     (render-to-texture) get **scaled tiles on demand**, keyed by VRAM
     rectangle, and are seeded from native when first needed.
   - Texture pages and CLUTs are sampled from native R16UI (as today).
   - Memory goes from about 1.3 GB plus mirrors down to the output target plus
     a few tiles.
5. **One pass per primitive.** PGXP depth is written in the colour pass
   (vertex depth, a single depth test), not a depth-only replay. Depth and
   threshold clears are scissored `glClear`, not geometry. Batches use a
   persistent-mapped ring buffer; no per-batch `glBufferData`.
6. **Higher frame rates come from the presentation world, not from re-running
   the game.** Today R4 re-executes the game's own drawing code with car and
   camera fields moved part-way (`src/mods/r4_interp.c`, with a hand-mapped
   field list). That's a title-specific hack of the PS1 camera, and it puts
   checkpoints and GPU backup/restore on the guest path. Replace it with
   **generic frame-packet interpolation** on the render side:
   - Each packet carries, per 3D primitive, a stable **identity** (PGXP
     provenance: GTE source and ordering-table position, or whatever key the
     code shows is stable across double-buffered packets) and its
     **view-space vertices** (the GTE's pre-projection coordinates PGXP
     already tracks), plus the projection constants (H, OFX, OFY).
   - For an in-between image at fraction t, the presentation world matches
     primitives between packet N-1 and N, interpolates view-space positions,
     and projects them. That's correct perspective motion for cars, track
     and camera, with no knowledge of the camera.
   - Unmatched primitives, 2D/HUD primitives, colours and UVs take packet N.
     Large deltas (cuts, respawns) snap, so there's no interpolation that frame.
   - Latency is the same as today (interpolating the last two ticks). Runs at
     any rate (60/90/120/144) on the render thread or a worker that builds
     vertex buffers; the guest thread does nothing extra.
   - Car Effects and other title metadata ride in the packet and are
     interpolated the same way, so they never force a serial path.
   - Generic across titles. The R4 interp mod, its field list, the render-pass
     checkpoint/transaction machinery and the private draw workers are
     deleted once the generic path matches or beats them. Match bar: **100%**
     of 3D primitives that exist in both packets are matched; a miss there
     is a test failure. Only primitives that truly appear or disappear
     between ticks go unmatched, and those must not visibly pop (verified
     on screen captures and by a per-frame unmatched counter).
7. **Snapshots are native-only and cheap.** Snapshot = CPU + RAM + devices +
   native VRAM, all CPU memory, taken at a frame boundary with dirty-page
   deltas. This is cheap enough for every frame, which is what Forza-style
   rewind needs. After a load, the presentation tiles re-seed from native.
8. **Audio is decoupled.** SPU output goes through a lock-free SPSC ring (no
   `SDL_LockAudioDevice` contention). The emulation thread and audio thread
   get MMCSS ("Games" and "Pro Audio") priority on Windows.
9. **Release builds carry no debug cost.** Store hooks, image rings and
   trace readbacks are compiled out (`PSX_NO_DEBUG_TOOLS`); the performance
   target is checked on that build.

## What gets deleted

GL_RT_SYNC on runtime paths; rt_acquire and rt_drain on the guest path; GL
context migration; public/private VRAM publication copies; the GPU-to-native
pack path; guest-path `glReadPixels`; full-VRAM HR colour and depth (1,296
MiB); canonical-plus-wide double submission; depth-only replay; quad depth
clears; extra-frame CPU checkpoints and GPU backup, capture and restore;
blocking present slots.

## Constraints kept (FRAME_PIPELINE section 9)

Exact native VRAM semantics (wrap, mask bit, semi-transparency, overlapping
copies) live in the native rasterizer, which is the reference already used
for HD authority. PGXP output is unchanged and never turned off. Savestate,
rewind and netplay determinism depend only on the guest world. HD texture
keys hash native bytes. Mods keep their hooks; per-title metadata rides in
packets.

## Phases (each lands on its own fork topic branch, tested, then merged to r4/main)

| # | Phase | Removes | Done when |
|---|---|---|---|
| 1 | Native authority everywhere: native raster thread; all guest reads from native; delete guest-path GL syncs, readbacks and context moves | rt_drain, rt_acquire, context switching, pack path | No guest-path GL wait in a race (counter = 0); pixel-exact GPUREAD and savestate tests pass |
| 2 | Mailbox presentation, bounded packet queue, lock-free audio ring, MMCSS | backpressure into guest time | Guest holds 60.0 VBlanks/s and 0 starved samples even with the GPU artificially slowed |
| 3 | Surfaces: direct wide target, on-demand RT tiles, one-pass PGXP depth, scissored clears, persistent buffers | 1.3 GB HR VRAM, double submission, depth replay | 8K60 GPU frame time well under 16.7 ms with all features on |
| 3b | PGXP correctness audit (below) | shimmer, flicker, depth fighting | All PGXP stability tests pass; owner-visible shimmer and flicker gone |
| 4 | Generic frame-packet interpolation (principle 6); delete r4_interp guest re-execution, transactions and workers | checkpoints, backup and restore, serial path, title-specific camera hack | 60 (and 120) distinct images/s at 8K with Car Effects on; 100% match of primitives present in both packets, no visible pops |
| 5 | Per-frame native delta snapshots, then Forza-style hold-to-rewind (reverse playback, release to resume) | Rewind snapshot cost | Rewind on costs < 0.5 ms per frame; hold rewinds smoothly |

## PGXP correctness audit (phase 3b)

The owner sees shimmering and flickering. PGXP must be verified correct on
every front, not assumed. Build measurable checks, find each root cause and
fix it (never by turning PGXP or a sub-feature off as the answer):

- **Determinism:** the same savestate replayed twice must give byte-identical
  presented images and identical PGXP vertex streams.
- **Temporal stability:** with the game paused, or on a static scene,
  consecutive images must be identical. Any pixel change there is a bug: a
  cache miss flipping a vertex between precise and integer, seam snapping
  toggling, depth threshold clears changing domains, or z-fighting.
- **Per-frame counters:** vertices that fall back to integer (precision cache
  misses or invalidations), seam-snap decisions that change from frame to frame
  for the same primitive, depth-domain resets, and pixels failing the depth
  test by less than epsilon (z-fighting).
- **Accuracy:** projected PGXP vertices must agree with the integer GTE
  result to within 1 native pixel. Check perspective texturing (w) for
  sanity on every primitive kind, and colour correction.
- **Lifecycle:** the precision cache stays correct across savestate loads,
  rewind, extra frames and VRAM uploads.
- Include Car Effects (particles, lighting) in the flicker checks.
- Keep the checks as regression tests or debug-server counters.

Target after phase 4: native 8K (18x), 60 FPS, Rewind, Car Effects, full
PGXP, on a Release build, steady 60.0 VBlanks/s, zero starved samples,
max VBlank interval under 20 ms, and 60 distinct images/s.

## Implementation notes (phase 1 in progress)

The native queue must receive data before the presentation facade. In the
current parser, `gp0_exec_shaded_tri`/`gp0_exec_shaded_quad` truncate RGB888 to
RGB555 and apply widescreen/HUD transforms before calling `gr_draw_*`.
Duplicating those facade calls on a CPU thread would preserve transformed,
already-quantized data rather than the exact native command. The capture point
therefore needs to precede those operations. The existing software raster's
missing dither input and colour interpolation limitations are also recorded in
`psxrecomp/accuracy/axis5_gpu.md`; its current use for HD keys is not proof of
full hardware pixel accuracy.

Partial GP0(A0) payloads are independently observable before the full upload
rectangle completes. The native queue now supports payload fragments with a
pixel offset and mask state; snapshots can retain the exact received prefix,
including wrapping and an odd final halfword. Tests cover taking and restoring
a snapshot between fragments. Runtime routing now captures original GP0
packets before presentation transforms and RGB555 truncation. GPUREAD, native
scanout, snapshots and restore use the native worker. GPU snapshots include
texture/palette cache contents, including intentionally stale valid entries.

The phase-1 topic branch starts at fork `r4/main` (`ac65fcf5`). The earlier
`fix/queued-vram-snapshot` commits remain preserved separately: their GL-owner
completion mechanism is not being made the new authority. Phase 5 should use
native CPU completion instead. No topic has passed its phase acceptance gate
or been merged as part of this implementation yet.

Native polygon groundwork (`643af69d`) now retains RGB888 and applies integer
attribute interpolation and guest dithering in a separate native-only entry
point. The existing presentation raster is unchanged. Hand-known gradient and
dither fixtures pass. Subsequent work integrated production routing and compared
10000 mixed draw cases, 10000 signed-coordinate cases, 2000 interlaced draw cases
and 2000 fill/copy cases against a pinned separate Beetle GPU oracle. This is
tested coverage, not a claim of exhaustive hardware equivalence. Native GPUREAD
also retains the adjacent high halfword of an odd transfer. A write-invalidated
CPU row cache avoids a worker wake per halfword.

Phase 1 is not accepted yet: configuration and diagnostic GL context handoffs
remain, as do presentation transactions. The phase-2 queue work must retain
native upload/copy effects when dropping a presentation packet; dropping
records from the existing incremental ring without independent packet inputs
would corrupt subsequent images. Component test success is not phase acceptance.

WIP `1f5f6e1a` queues frequently used controls and CPU presentation images,
including immutable overlays/display configuration. Diagnostic snapshots avoid
context moves for frequently polled counters. Large transport records are
fragmented and assembled on the render owner; no oversized-image fallback
runs GL on the guest thread. This transport still has backpressure, explicitly
left for phase 2. Three hidden all-on runs recorded zero context acquisitions,
but legacy control/diagnostic/transaction APIs still require migration or
deletion before the ownership audit is complete.

### Phase-1 integration boundary (2026-10-28)

The native-authority acceptance fixtures and race context-migration counter
now pass at framework `464bc104`: original GP0, partial uploads, native
GPUREAD/scanout, cache-preserving snapshots, and zero acquisitions/releases
through the all-on 18x race. Native display history no longer performs GPU
readback; pure-render diagnostics run as copied requests on the render owner.

Sequencing detail: the existing incremental presentation ring still blocks
at its two-frame limit. Removing that wait is phase 2, not a claim of this
phase. Legacy non-race ownership APIs (live allocation, texture banks, XR and
transaction diagnostics) remain to migrate with independent packet/control
ownership; transaction APIs themselves remain until their specified phase-4
replacement. No fallback has been added. HD presentation tracking still uses
its ordered presentation mirror; independent packets must carry exact native
texture inputs rather than promote that mirror back to guest authority.

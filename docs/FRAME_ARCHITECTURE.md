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
6. **Extra frames are packets, not transactions.** 60 FPS in-between images:
   a worker runs the interpolated guest draw recipe on a copy-on-write
   context and emits a frame packet, and the render thread draws it into its
   own presentation target. Because presentation targets are never canonical,
   no GPU backup, capture or restore and no main-thread checkpoint are
   needed. Car Effects metadata travels per packet, so it no longer forces
   the serial path.
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
| 4 | Extra frames as worker packets with per-packet Car Effects metadata | checkpoints, backup and restore, serial path | 60 distinct images/s at 8K with Car Effects on |
| 5 | Per-frame native delta snapshots, then Forza-style hold-to-rewind (reverse playback, release to resume) | Rewind snapshot cost | Rewind on costs < 0.5 ms per frame; hold rewinds smoothly |

Target after phase 4: native 8K (18x), 60 FPS, Rewind, Car Effects, full
PGXP, on a Release build, steady 60.0 VBlanks/s, zero starved samples,
max VBlank interval under 20 ms, and 60 distinct images/s.

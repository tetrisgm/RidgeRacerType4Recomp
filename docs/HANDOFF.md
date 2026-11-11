# Handoff

## Current work — 2026-11-11

Use this Mac checkout on `master`; read `CLAUDE.md` and
[the renderer development receipt](recovery/SEPARATE_RENDERER_20261111.md).
The goal remains 60 distinct frames/s at fixed 2160 lines, retaining MStan's
performance work, the custom Select/Back menu, settings/controls, Steam
Controller fixes, full PGXP, Max Detail and effects. NAS and STEAMBOX are not
build or test machines. The owner authorized engineering decisions and visible
race verification; other automation stays hidden with isolated saves/settings.

## Result and blocker

**4K/60 remains unqualified; no new package.** The independent scene snapshots,
interpolation/clipping and private GPU backend pass fixtures, but the R4 scene
producer is not connected. Do not call this a finished separate race renderer.
The current race still uses guest redraws with CPU/device rollback. Its optional
fresh-target experiment avoids native high-resolution backup/restore copies,
now correctly handling both alternating vertical buffers.

Fixed-9x, 21:9 (5040x2160), 120-second throughput improved from 36.33 to 47.38
VBlanks/s with the buffer fix, then **55.41** with queued wide draws restricted
to visible side margins. This is below steady 60. Shader, translucent-bloom and
direct-capture probes established no useful gain and were removed. GPU tracing
identified fragment work as the larger GPU cost. Measurement limits and hashes
are in the receipt; do not substitute swap counters for distinct-frame proof.

Six same-state full-resolution pairs before/inside/after the tunnel match
exactly. Visible-run sky, ground and HUD captures, plus twelve successive tunnel
images, show no gaps or blinking in the inspected samples. This is bounded
coverage, not an exhaustive all-course or physical-display certification.

## Verification and next work

Release and 38 CTests pass, including 144 private-target/ambient, 42 opaque-batch
and 75 wide-margin GL checks. Final menu/controls/settings/boot-to-race smoke passes with zero dispatch
misses. The hidden harness explicitly disables VSync to avoid Cocoa throttling
hidden windows to about 1 FPS. Engine pin is `5d379876` (status-only follow-up to `c593b788`; implementation `1f5030c5`); supported game
regeneration leaves generated C unchanged and reuses all 7,459 overlay variants. The UI pin remains `ef8150f5`.

The post-margin CPU-only profile still finds dominant render-owner waits in
OpenGL/Metal presentation. A smaller private-target probe did not reproduce
its initial gain and was removed; later runs varied substantially. See the
receipt before interpreting the 55.41 measurement as repeatable capacity.

Later probes did not establish a useful gain and were removed: single-target
redraw measured 56.99 VBlanks/s with nonzero pixel differences; bounded wide
depth clears matched reference images but measured 51.46. The retained
checkpoint at standard 16:9/3834x2160 measured 59.38 over 120 seconds, with
54–56 FPS dips. Neither aspect qualifies steady 60. Details are in the receipt.

Next: complete the R4 pre-cull geometry/material producer before connecting
the independent backend, then qualify the complete presentation path.
Preserve texture LOD mode 1 and anisotropy 16. Fresh targets remain debug-only,
off by default (`PSX_RENDER_PASS_FRESH_TARGET=1`); they are not qualified for
partial-frame or old-image feedback. Margin clipping is on by default under
the canonical-center contract; `PSX_GL_WIDE_MARGINS_ONLY=0` is the reference.

## References

- [Renderer work, tests and receipts](recovery/SEPARATE_RENDERER_20261111.md)
- [Earlier frame-rate investigation](recovery/FRAME_RATE_20261110.md)
- [Threading/history evaluation](recovery/THREADING_EVALUATION_20261109.md)
- [Menu restoration](archive/CUSTOM_MENU_20261109.md)
- [Upstream reviews](UPSTREAM.md) / [pin history](framework_pin_history.md)
- [Windows packaging](WINDOWS_BUILD.md)

Historical sources remain at `/Users/shokunin/dev-archives/r4-20261109/`.
The older native-30 Windows package is not the current renderer checkpoint.

# Rewind release pipeline fix — 2026-11-12

Engine `d81b3919`, game integration `63f58850` (0.2.2).

The old restore cleared producer geometry and discarded GPU work already issued
at the selected machine boundary. Normal rendering then had to refill the
pipeline. Main-thread full-rate planning also fell back to a wall-clock planner
whose schedule had just been cleared. Checkpoint spacing added avoidable replay.
The measured next producer was issued 1,980 guest cycles before the restored CPU
boundary; restoring it does not introduce future inputs.

The renderer now restores only eligible current-branch producers by exact issue
cycle, uses the same VBlank cadence with either GL context owner, and retains
two full-resolution working textures. Safe state anchors shorten replay within
the existing bounded history. Presentation remains gated by tick, source cycle
and phase until forward rendering catches up. The selected guest state is exact.
The real-GL fixture rejects future-issued commands, discarded branches and
older phases; checks pixels and CPU/GPU/depth/stencil isolation at 1x/4x/9x; and
checks Mac worker drawable lifecycle. No menu/controller/PGXP implementation was
changed.

A Cocoa drawable-update crash during qualification implicated two shared contexts
attached to one visible NSView. Composition now uses its own hidden drawable;
only the presentation context attaches to the visible window. Both workers stay
enabled. Subsequent native and signed-bundle runs pass.

All 44 tests, regenerated static overlays, hidden muted smoke, menu bindings and
settings restart pass. The two-minute Mac run averages 59.86 VBlanks/s with all
3,599 intermediate frames and maximum guest-frame interval 35.542 ms. Five
internal releases are 27–47 ms, with input polling observing 31–65 ms. A final
version-stamped binary check observes 35–55 ms internally. These are finite
measurements, not zero-latency or all-course claims.

Three exact diagnostic restores differ in zero RAM words; 130 full-resolution
images match. The signed package adds two exact restores and 96 matching images.
Route captures cover tunnel, sky and ground; consecutive resume images retain
resolution and HUD ordering. The packaged app was also observed on screen.
Audio remained muted; physical controller/listening tests are separate.

See [Mac receipt](MAC_RELEASE_20261112.json), [current handoff](../HANDOFF.md),
and the [earlier unsuccessful investigation](REWIND_RELEASE_20261112.md).
Local detailed evidence is under `analysis/rewind-final-*20261112/` and
`analysis/mac-022-*20261112/`; game data and captures are not committed.

Native PC qualification also passes: 59.96 VBlanks/s, 3,598/3,598 intermediate
frames, three exact state restores and 108 matching images. Internal release
times are 42–72 ms. [Windows receipt](WINDOWS_RELEASE_20261112.json) records
the result and complete temporary security/task/file cleanup.

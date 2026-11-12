# Rewind release investigation — 2026-11-12

Status: unresolved. No runtime change qualified or shipped. Existing 0.2.1
packages were not replaced. All runs used dummy audio and isolated player data.
No PC access or security changes occurred in this follow-up.

## Evidence

Local machine-readable comparison:
`analysis/rewind-release-investigation-20261112/summary.json`.
It links the full per-run receipts and separates diagnostic from normal timing.
The rejected final experiment patch is retained beside it for investigation only.

The baseline reproduced five release transitions of 74–130 ms. State load was
roughly 7–12 ms; restore plus replay took 21–36 ms, followed by 37–80 ms until
the first complete forward image. These stages explain why timing RAM copies
alone does not measure the user-visible pause. Polling adds observation overhead.

Experiments covered retaining GL ownership through refill, denser checkpoints,
reconstructing hidden private draws, extending the unpaced refill bound, rejecting
incomplete composition earlier, and omitting hidden cosmetic triangle rasterization
while preserving native rasterization. None established a reliable material gain.
The extended-refill/context experiment produced a 6.54-second image hold and was
rejected. The direct same-tick checkpoint experiment was not evaluated: its changed
execution identity correctly rejected the existing grid state; no guard was bypassed.

The final native-raster experiment passed three exact RAM restores and its enabled
full-resolution image-parity diagnostic, but normal transitions still measured
74–122 ms. That is not a fix for the requested pause. Its diagnostic frame rate
must not be used as normal performance evidence. All experimental source and test
edits were reverted to engine a6f0b4b5; no menu/controller code was changed.

## Baseline verification after reverting experiments

Release rebuild and all 44 CTests passed, including three real-GL fixtures.
The muted custom-menu/settings/persistence and boot-to-race smoke check passed
with 7,172,567 static overlay hits and zero dispatch misses. Receipt:
`analysis/rewind-release-baseline-check-20261112/native-overlay-check.json`.
These checks validate the preserved baseline, not removal of the release pause.

## Remaining work

The complete-image gate must stay: releasing native restaging to the screen would
reintroduce the previous low-resolution center/HUD problem. Further work should
measure the forward producer refill at individual stages and investigate restoring
its renderer state alongside the selected machine state. Any retained commands
must belong to that restored guest-cycle boundary, not later recorded gameplay.
This is a proposed direction, not a validated implementation.

Do not publish a new package or claim this release-pause requirement passed based
on the existing 0.2.1 performance/visual receipts. Physical controller and audible
sound checks remain separate, as recorded in the handoff.

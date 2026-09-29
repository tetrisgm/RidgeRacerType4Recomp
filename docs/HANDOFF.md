# Handoff

## Current work

v0.1.0 (preview) released 2026-09-26: setup-host zips for macOS (universal,
ad-hoc signed) and Windows x64 at
https://github.com/tetrisgm/RidgeRacerType4Recomp/releases/tag/v0.1.0 (repo
public). Built with `tools/package_release.sh` on the Mac and on the PC (MSYS2);
player flow verified end to end headlessly on both. Pending: the owner's GUI
click-through of the setup wizard.

Widescreen: the default-off `r4.enhancement.widescreen` mod
(`docs/WIDESCREEN.md`) on psxrecomp master (the cull kinds and line batching
of RetroPortingToolKit/psxrecomp#422); the carried `renderer/adaptive/` is
retired.

Frame rate: the default-off `r4.enhancement.frame-rate` mod (Display refresh
or 60–300 FPS; Interpolated via psxrecomp render passes, or Frame blend; logic
stays 30 Hz), on psxrecomp#421/#423/#424.

Internal resolution (Native to true 8K): R4 PR #4 is ready on psxrecomp
master (#425–#427) and waits only for RetroPortingToolKit/recomp-ui#71 (the
Settings row); then re-pin recomp-ui to the merged master and merge #4 with
the recorded resolution (R4 `README.md`, pin history).

## Blockers

None.

## References

- Disc identity: `DISC.md` (Redump 11608).
- Framework/UI pins: `docs/framework_pin_history.md`.
- Widescreen: `docs/WIDESCREEN.md`; cull lists `tools/r4_ws_scan.py`.
- Frame rate: `README.md` (Frame rate) and the package README; field table
  `tools/gen_r4_interp_fields.py`.
- Verification loop: `tools/run_r4.sh build` + `tools/smoke.py <out-dir>`;
  rollback determinism: `PSX_RB_SELFCHECK=1 PSX_RB_SELFCHECK_MASH=1`.
- A/B guest identity (mod off, warm vs cold shards):
  `psxrecomp/tools/fp_identity.py` with
  `--launch 'tools/run_r4.sh {build} --debug-port {port} {headless}'`.

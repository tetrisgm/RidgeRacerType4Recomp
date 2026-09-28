# Handoff

## Current work

v0.1.0 (preview) released 2026-09-26: setup-host zips for macOS (universal,
ad-hoc signed) and Windows x64 at
https://github.com/tetrisgm/RidgeRacerType4Recomp/releases/tag/v0.1.0 (repo
public). Built with `tools/package_release.sh` on the Mac and on the PC (MSYS2);
player flow verified end to end headlessly on both. Pending: the owner's GUI
click-through of the setup wizard.

Widescreen: branch `feat/adaptive-widescreen` (this repo) with psxrecomp
`feat/adaptive-widescreen` (on upstream master) adds the default-off
`r4.enhancement.widescreen` mod (`docs/WIDESCREEN.md`) and retires the
carried `renderer/adaptive/`. Re-pin psxrecomp after the upstream merge.

## Blockers

None.

## References

- Disc identity: `DISC.md` (Redump 11608).
- Framework/UI pins: `docs/framework_pin_history.md`.
- Widescreen: `docs/WIDESCREEN.md`; cull lists `tools/r4_ws_scan.py`.
- Verification loop: `tools/run_r4.sh build` + `tools/smoke.py <out-dir>`;
  rollback determinism: `PSX_RB_SELFCHECK=1 PSX_RB_SELFCHECK_MASH=1`.

# Handoff

## Current work

v0.1.0 (preview) released 2026-09-26: setup-host zips for macOS (universal,
ad-hoc signed) and Windows x64 at
https://github.com/tetrisgm/RidgeRacerType4Recomp/releases/tag/v0.1.0 (repo
public). Built with `tools/package_release.sh` on the Mac and on the PC (MSYS2);
player flow verified end to end headlessly on both. Pending: the owner's GUI
click-through of the setup wizard. Next feature (later): widescreen via the
adaptive renderer in `renderer/adaptive/`, as a worktree pair with psxrecomp PRs.

## Blockers

None.

## References

- Disc identity: `DISC.md` (Redump 11608).
- Framework/UI pins: `docs/framework_pin_history.md`.
- Adaptive renderer carry-over: `renderer/adaptive/README.md`.
- Verification loop: `tools/run_r4.sh build` + `tools/smoke.py <out-dir>`;
  rollback determinism: `PSX_RB_SELFCHECK=1 PSX_RB_SELFCHECK_MASH=1`.

# Handoff

## Current work

Distribution (`feat/distribution`, PR tetrisgm/RidgeRacerType4Recomp#1, draft):
setup-host zips for macOS (universal, ad-hoc signed) and Windows x64, built by
`tools/package_release.sh` on the Mac and on the PC (MSYS2). Player flow tested
end to end on both headlessly; GUI click-through pending with the owner.
Needs upstream RetroPortingToolKit/psxrecomp#393, #394, #395; until they merge
the psxrecomp gitlink points at `tetrisgm/psxrecomp` `integration/distribution`
(build-only). Worktrees: `~/dev/ridgeracertype4-wt/distribution` (Mac) and
`C:\Users\Shokunin\dev\ridgeracertype4-wt\distribution` (PC, synced by git
bundle).

## Blockers

None.

## References

- Disc identity: `DISC.md` (Redump 11608).
- Framework/UI pins: `docs/framework_pin_history.md`.
- Adaptive renderer carry-over: `renderer/adaptive/README.md`.
- Verification loop: `tools/run_r4.sh build` + `tools/smoke.py <out-dir>`;
  rollback determinism: `PSX_RB_SELFCHECK=1 PSX_RB_SELFCHECK_MASH=1`.

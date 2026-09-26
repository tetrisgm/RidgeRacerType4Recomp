# Handoff

## Current work

Bring-up complete and pushed (see `ISSUES.md` header for what is verified).
Builds on macOS arm64 (`~/dev/ridgeracertype4`) and Windows x64 on the PC
(`C:\Users\Shokunin\dev\ridgeracertype4`, MSYS2 MinGW64; its checkout is
synced by git bundle because the PC has no GitHub credentials). The Windows
build passed the headless turbo smoke test to a race.
Next: owner check-in on feature enhancements — candidates are an R4
widescreen plugin on `renderer/adaptive/`, DualShock analog as default,
AOT overlay shards for R4.BIN, SIO1 link cable, a full-playthrough soak, and a
distributable macOS build.

## Blockers

None. Open decisions for the owner: repository license (none chosen yet).

## References

- Disc identity: `DISC.md` (Redump 11608).
- Framework/UI pins: `docs/framework_pin_history.md`.
- Adaptive renderer carry-over: `renderer/adaptive/README.md`.
- Verification loop: `tools/run_r4.sh build` + `tools/smoke.py <out-dir>`;
  rollback determinism: `PSX_RB_SELFCHECK=1 PSX_RB_SELFCHECK_MASH=1`.

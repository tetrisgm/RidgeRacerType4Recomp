# Handoff

## Current work (2026-10-08)

Released: v0.1.0 only. **Everything planned for the next release is merged;
only the release itself is left**, waiting on the owner's final go and version
number. Never tag or publish without that ask.

Merged on master: R4 #6-#13, #15, #17 (display defaults; #9 folded in and
closed), #18 (bundled compiled releases), #10 (online 2-4 players), #20 (HD
HUD), #21 (player guide + draft release notes). Pins: psxrecomp `67a21b73`
(#583, all framework PRs this release needed), recomp-ui `7e884a2`.

No open R4 or psxrecomp PRs gate the release.

Release dry run (local only, not published): `analysis/handoff/rc-dry2-20261008/`
(gitignored) holds the macOS arm64/x64 and Windows x64 zips from R4 master
`8d360fa` and the check results.

Owner test build: `~/dev/ridgeracertype4-wt/pacing-r4`, branch
`test/owner-build-2` (R4 master), shipped defaults; race savestates slots
1-4 (1P) and 9 (2P VS). The previous build's saves are backed up beside it.

## Owner decisions

- Display: widescreen Fit, Match display, supersample 1.5, FXAA off, dynres
  floor = display, render + present threads, Smooth motion with reprojection,
  PGXP depth buffer / colour correction / fine seams. All in `game.toml`.
- `guest_cycle_scale = 2` (races, Max Detail gate).
- HD HUD bundled, on by default, credited to Kuid0us.
- Online: 2-4 players, each on their own full-screen view.
- Rewind off in split screen and in every online session.
- Release model: bundled compiled zips (#18). Version: TBD.

## Blockers

- Release: owner picks the version and gives the go.
- Online battle keeps its experimental switch until psxrecomp reserves link memory per netplay session.

## References

- Disc identity: `DISC.md` (Redump 11608).
- Framework/UI pins: `docs/framework_pin_history.md`.
- Widescreen: `docs/WIDESCREEN.md`; cull lists `tools/r4_ws_scan.py`.
- Display defaults and Smooth motion: `README.md` (On by default, Smooth
  motion); `tests/test_r4_defaults.py`, `tools/check_pin_keys.py`.
- HD HUD: `docs/HD_HUD.md`, `tools/r4_hd_hud_pack.py`.
- Internal resolution presets: `README.md` (Internal resolution), `tools/res_matrix.py`.
- Release: `tools/package_release.sh` (gates: OpenBIOS only, HD HUD pack exact, pin keys).
- Verification loop: `tools/run_r4.sh build` + `tools/smoke.py <out-dir>`;
  rollback determinism: `PSX_RB_SELFCHECK=1 PSX_RB_SELFCHECK_MASH=1`.
- A/B guest identity (mod off, warm vs cold shards):
  `psxrecomp/tools/fp_identity.py` with
  `--launch 'tools/run_r4.sh {build} --debug-port {port} {headless}'`.

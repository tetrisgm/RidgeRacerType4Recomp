# Handoff

## Current work (2026-10-09)

Released: v0.1.0 only. Next release waits on the open PRs below landing and
the owner's go and version number. Never tag or publish without that ask.

Merged since 2026-10-08: R4 #29 (analog sweep test), #30 (forgiving analog
response curves), #33 (remote-seat test tooling).

Open, waiting on psxrecomp review/merge (all review points answered):
- Visual: psxrecomp #584 dither (stacked on #587), #586 xBR, #587 accurate
  blending, #589 bloom.
- Performance: #590 v_wait idle skip, #595, #596, #597 -> R4 #22, #25.
- Presets: #591 + recomp-ui #86 -> R4 #23 (Low on low-end, Ultra otherwise).
- Linux/Deck: #592, #593 -> R4 #24.
- In-game menu: #598-#603 -> R4 #26 -> #27 -> #28 (tetrisgm/recomp-launcher).
- Controls: #608, #610 -> R4 #31 (rumble); #609 -> R4 #32 (wheel FFB).
- Netplay: #611 (false desync warnings); #607 spectators (inert until the
  lobby server relays spectators; #612 and recomp-net #34 are drafts).

## Owner decisions

- Display: widescreen Fit, Match display, supersample 1.5, FXAA off, dynres
  floor = display, render + present threads, Smooth motion with reprojection,
  PGXP depth buffer / colour correction / fine seams. All in `game.toml`.
- `guest_cycle_scale = 2` (races, Max Detail gate).
- HD HUD bundled, on by default, credited to Kuid0us.
- Online: 2-4 players, each on their own full-screen view.
- Rewind off in split screen and in every online session.
- Release model: bundled compiled zips (#18). Version: TBD.
- Graphics presets: low-end machines start Low (extras off); others Ultra
  with dynamic resolution, frame rate and aspect.
- Modern controls: NeGcon analog gas/brake/steering, forgiving curves.
- New launcher: tetrisgm/recomp-launcher replaces recomp-ui, minimal R4 menus.

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

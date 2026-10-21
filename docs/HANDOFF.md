# Handoff

## BRIEF-4 (2026-10-21)

Stage 1 is accepted on `fix/slowdowns` in R4 and psxrecomp.
Quiet/loaded completion checks pass; title ctest passes 21/21.
[Results and receipts](../analysis/STATUS.md); [owner brief](../analysis/BRIEF-4.md).
Stage 1 branches are pushed (R4 `6355416`, framework `766f9d72`).
Stage 2 is in progress on `fix/draw-distance`: full-circuit/range and qualified
far-projection changes, 8 MB RAM/expanded heaps, HUD bounds and visibility/material
caches are implemented. Timing acceptance still fails; bloom-pass fusion is in
validation. The reported drifting-building landmark/frame remains unverified;
matching route 800/1202 captures alone do not establish that visual fix.
Stage 3 has not started. Prior dirty work is preserved on
`wip/draw-distance-old` in both repos. No PR, merge or release is authorized.

## Current work (2026-10-14)

Released: v0.1.0 only. Next release waits on the psxrecomp PRs below, then a
rebuild from master and the owner's go and version number. Never tag or
publish without that ask. Local RC report: `analysis/handoff/rc-1012/RC_REPORT.md`.

On R4 master: psxrecomp master pin (dithering off, bloom on, #38), Linux zip
(#24), Modern rumble (#31), link-bridge snapshot fix (#39), widescreen VBlank
cache (#37).

Open R4 PRs -> psxrecomp PR they wait on:
- #22 v_wait idle skip (audited site list) -> #590.
- #23 graphics presets (Low on low-end, Ultra otherwise; bloom off in Low) -> #591.
- #25 batched cycle charge -> #597.
- #26 -> #27 -> #28 -> #36 recomp-launcher in-game menu -> #598-#603
  (provider-neutral `psx_menu_host.h`; recomp-ui stays default, R4 opts in).
- #32 JogCon wheel force feedback (off by default) -> #609 (needs a physical wheel test).
Also open: #593 Steam Deck Game Mode (needs real Deck); #660-#662 4P rollback
determinism (issue #636).

Merge order once upstream lands: re-pin each R4 PR to psxrecomp master; keep
both sides of `docs/framework_pin_history.md` when rebasing.

## Owner decisions

- Display: widescreen Fit, Match display, supersample 1.5, FXAA off, dynres
  floor = display, render + present threads, Smooth motion off (removed,
  owner 2026-10-20), PGXP depth buffer / colour correction / fine seams. All in `game.toml`.
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
- Display defaults: `README.md` (On by default); `tests/test_r4_defaults.py`,
  `tools/check_pin_keys.py`.
- HD HUD: `docs/HD_HUD.md`, `tools/r4_hd_hud_pack.py`.
- Internal resolution presets: `README.md` (Internal resolution), `tools/res_matrix.py`.
- Release: `tools/package_release.sh` (gates: OpenBIOS only, HD HUD pack exact, pin keys).
- Verification loop: `tools/run_r4.sh build` + `tools/smoke.py <out-dir>`;
  rollback determinism: `PSX_RB_SELFCHECK=1 PSX_RB_SELFCHECK_MASH=1`.
- A/B guest identity (mod off, warm vs cold shards):
  `psxrecomp/tools/fp_identity.py` with
  `--launch 'tools/run_r4.sh {build} --debug-port {port} {headless}'`.

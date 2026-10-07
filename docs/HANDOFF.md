# Handoff

## Current work

Released: v0.1.0 (2026-09-26, setup-host zips). Master has widescreen (#2),
Frame Rate (#3), internal resolution (#4) and near-wall docs (#5).

Open R4 PRs (2026-10-06), dependencies in each PR body:
- #6 hide rear-view mirror (per player online); #7 camera look-around (on by default; off online); #8 JogCon + analog default (off online);
- #10 online battle, 2-4 players, each sees their own car full screen in their own widescreen aspect; Modern controls online (experimental switch remains) — psxrecomp #512, #535, #542, #545, #549, #511; recomp-net #26, #28;
- #11 Max Detail; #12 VS split screen; #13 PGXP default (#513) — Max Detail and PGXP stay off online;
- #15 Controls (Modern default on all 4 players, Classic toggle; Rewind off in split screen and online) — psxrecomp #519-#522, #533, #549; recomp-net #27, #28;
- #17 display defaults on the render-thread pipeline (includes #9; frame generation off until camera interpolation is accepted) — psxrecomp #536-#540, #547, #548; recomp-ui #82;
- #18 bundled compiled releases (owner decision 2026-10-06): after it lands every R4 PR regenerates `generated/` at its re-pin.
Closed/superseded: R4 #9, #14, #16; psxrecomp #514-#518, #530, #531 parked drafts; #539's triangle matching superseded by #547.

Merge order: recomp-net #25, #26, #27, #28; psxrecomp #506, #507, #511,
#512, #513, #519-#522, #533, #534, #535, #542, #545, #549, #508, #532, #536,
#537, #538, #539, #540, #547, #548; recomp-ui #79, #82; then R4 #18, #6, #7,
#8, #11, #12, #13, #15, #17, #10 (re-pin + regen each).

## Blockers

- Owner: enable frame generation (camera interpolation, #547) by default? 2P tunnels not yet driven.
- Online battle keeps its experimental switch until psxrecomp reserves the link memory per netplay session.
- Mac<->PC LAN online test pending (`ssh pc` unreachable from the current network).

## References

- Disc identity: `DISC.md` (Redump 11608).
- Framework/UI pins: `docs/framework_pin_history.md`.
- Widescreen: `docs/WIDESCREEN.md`; cull lists `tools/r4_ws_scan.py`.
- Frame rate: `README.md` (Frame rate) and the package README; field table
  `tools/gen_r4_interp_fields.py`.
- Internal resolution presets: `README.md` (Internal resolution), `tools/res_matrix.py`.
- Verification loop: `tools/run_r4.sh build` + `tools/smoke.py <out-dir>`;
  rollback determinism: `PSX_RB_SELFCHECK=1 PSX_RB_SELFCHECK_MASH=1`.
- A/B guest identity (mod off, warm vs cold shards):
  `psxrecomp/tools/fp_identity.py` with
  `--launch 'tools/run_r4.sh {build} --debug-port {port} {headless}'`.

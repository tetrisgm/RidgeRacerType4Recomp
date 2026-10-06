# Handoff

## Current work

Released: v0.1.0 (2026-09-26, setup-host zips). Master has widescreen (#2),
Frame Rate (#3), internal resolution (#4) and near-wall docs (#5).

Open R4 PRs (2026-10-06), dependencies in each PR body:
- #6 hide rear-view mirror; #7 camera look-around (on by default); #8 JogCon + analog default;
- #10 online battle, 2-4 players, each on their own view (experimental switch until per-session memory reservation exists) — psxrecomp#512, #535, #511, recomp-net#26;
- #11 Max Detail; #12 VS split screen; #13 PGXP default — psxrecomp#513;
- #15 Controls (Modern default, Classic toggle; Rewind off in split screen and online) — psxrecomp#519-#522, #533, recomp-net#27;
- #17 display defaults on the render-thread pipeline (includes #9) — psxrecomp#536-#540 (#537 merges #508 and #532).
Closed/superseded: R4 #9 (in #17), #14, #16; psxrecomp #514-#518, #530, #531 parked as drafts.

Merge order: recomp-net #25, #26, #27; psxrecomp #506, #507, #511, #512, #513,
#519-#522, #533, #534, #535, #508, #532, #536, #537, #538, #539, #540;
recomp-ui #79 (#77, #80, #81 merged); then R4 #6, #7, #8, #11, #12, #13, #15,
#17, #10, re-pinning each to the merged framework.

Plan: `analysis/ROADMAP-2026-10-05.md` (local).

## Blockers

- Frame generation is wired but rarely admitted on this loaded host (breaker
  trips on late guest frames); needs a quiet-host measurement and tuning.
- Render/present thread and frame generation have no launcher toggle yet (env
  switches only); needs a psxrecomp/recomp-ui setting.
- Online battle keeps its experimental switch until psxrecomp can reserve the
  link memory per netplay session.
- Owner: release model (bundled `generated/` vs setup-host) before v0.2.0.

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

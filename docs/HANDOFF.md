# Handoff

## Current work

Released: v0.1.0 (2026-09-26, setup-host zips). Master has widescreen (#2),
Frame Rate (#3), internal resolution (#4) and near-wall docs (#5).

Open R4 PRs (2026-10-05), each with its framework dependencies linked in the
PR body:
- #6 hide rear-view mirror (default on);
- #7 camera look-around (default off) — needs RetroPortingToolKit/psxrecomp#506;
- #8 JogCon input and native analog default — needs RetroPortingToolKit/recomp-net#25, psxrecomp#507;
- #9 dynamic resolution on, 720p floor — needs psxrecomp#508, RetroPortingToolKit/recomp-ui#77;
- #10 (draft) experimental three/four-seat Link Battle — needs psxrecomp#512, #511, recomp-net#26, recomp-ui#80;
- #11 Max Detail (default on) — framework merged (#468);
- #12 VS split screen interpolation and texture-window batching — framework merged (#467);
- #13 PGXP on by default — needs psxrecomp#513;
- #14 Frame Rate leftover-time passes and Unlimited — needs psxrecomp#514→#518 (stacked);
- #15 Controls: Modern scheme on by default, Classic toggle — needs psxrecomp#519→#522 (stacked on #507), recomp-net#27 (on #25), recomp-ui#81;
- #16 display defaults on (widescreen Fit, Frame Rate display refresh, Match display + dynamic resolution, frame-rate priority) — stacked on #14, includes #9; needs psxrecomp#530 (one budget, merges #508), #531 (passes that run), #532 (widescreen at Match display holds 60).

Suggested merge order: recomp-net #25, #26, #27; psxrecomp #506, #507, #508,
#511, #512, #513, #514–#518, #530, #531, #532, #519–#522; recomp-ui #77, #79, #80, #81; then R4
#6–#16 (#16 supersedes #9), re-pinning each R4 branch to the merged framework.

Local only (dev Mac; details in `analysis/HANDOFF-LOCAL.md`, gitignored):
the held `feat/bundled-releases` branch (generated/ stays unpublished; the
lanes that were stacked on it are now #11–#13 on master).

## Blockers

- Frame Rate fills about half of game frames at Native and a third at 4K;
  more needs presents off the emulation thread. 2-4 frames per 8 s run land
  slightly late when a host stall follows a pass (psxrecomp#531).
- Not tested: physical controllers (modern controls, JogCon), launcher
  capture UI, 2P with modern controls.
- Owner: pushing `generated/` for bundled releases stays on hold.

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

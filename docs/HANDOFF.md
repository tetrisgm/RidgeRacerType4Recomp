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
- #10 (draft) experimental three/four-seat Link Battle — needs psxrecomp#512, #511, recomp-net#26, recomp-ui#80.

Suggested merge order: recomp-net #25, #26; psxrecomp #506, #507, #508,
#511, #512; recomp-ui #77, #79, #80; then R4 #6, #7, #8, #9, #10, re-pinning
each R4 branch to the merged framework.

Local only (dev Mac; details in `analysis/HANDOFF-LOCAL.md`, gitignored):
the held `feat/bundled-releases` branch, the Max Detail and VS-split lanes
stacked on it, and a release-candidate integration of all of the above.

## Blockers

- Owner: push `generated/` (it embeds the EXE's code words) for the bundled
  release model, or keep holding `feat/bundled-releases` and v0.2.0.
- Owner: modern controls need a framework design choice (extend the unified
  external-input layer with button remap, NeGcon pressure and a host-trigger
  read, or drop the feature).
- Owner: the opportunistic render-pass / frame-pacing lane conflicts with
  upstream's rebuilt render passes; port it as new work or drop it. The
  Unlimited-rate and pass-cost measurements wait on that.

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

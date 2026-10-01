# Handoff

## Current work

Released: v0.1.0 (2026-09-26, setup-host zips). Master has widescreen (#2),
Frame Rate (#3), internal resolution (#4) and near-wall docs (#5); they are
still default-off on master.

In progress (2026-10-01), on local branches on the dev Mac. The full state,
branch heads, worktrees and next steps are in `analysis/HANDOFF-LOCAL.md`
(gitignored; it exists only on that Mac):
- **Bundled releases (held):** branch `feat/bundled-releases` moves R4 to
  upstream's bundled compiled-release model (`generated/` committed, local
  packaging, OpenBIOS only, no CI), with v0.2.0 notes. Pushing waits for the
  owner.
- **Visual fidelity, all on by default for R4:** Max Detail (draw distance,
  no distance LOD), PGXP stability, 2P fixes, dynamic aspect (Fit), Frame Rate
  at display refresh (opportunistic, never slowing the game), Match display
  uncapped, and dynamic resolution. Framework PRs:
  RetroPortingToolKit/psxrecomp#467 (2P batching), #468 (draw-distance
  clamps), #470 (PGXP), #460 (overlay arch), #449 (release notices).
  Opportunistic render passes, pass cost and dynamic resolution are local and
  unreviewed.

## Blockers

- The owner must decide on pushing `generated/` (it embeds the EXE's code
  words) before the bundled-release branch, and v0.2.0, can land.
- Open bug: in a test build the image jumps back and forth between two moments
  during a race. It is probably in the new opportunistic Frame Rate work; see
  `analysis/HANDOFF-LOCAL.md` §4. Fix it before publishing that work.

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

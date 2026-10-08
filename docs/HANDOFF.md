# Handoff

## Current work (2026-10-08)

Released: v0.1.0 only. Next version number: owner TBD. No tag or release yet.

Merged on master: R4 #6-#13, #15, #18 (bundled compiled releases), #10
(online 2-4 players), #21 (player guide + draft release notes).

Open R4 PRs:
- [#17](https://github.com/tetrisgm/RidgeRacerType4Recomp/pull/17) display defaults on the render-thread pipeline (includes [#9](https://github.com/tetrisgm/RidgeRacerType4Recomp/pull/9));
  waits on psxrecomp #576 re-pin (+ `r4_defaults` test accepting dynres floor `display`).
- [#20](https://github.com/tetrisgm/RidgeRacerType4Recomp/pull/20) HD HUD (Kuid0us/T4HDHUD), on by default; waits on psxrecomp #566/#567 then re-pin.
- [#9](https://github.com/tetrisgm/RidgeRacerType4Recomp/pull/9) dynres 720p floor; folded into #17, close when #17 lands.

Open psxrecomp PRs, merge order:
1. [#558](https://github.com/RetroPortingToolKit/psxrecomp/pull/558) → [#559](https://github.com/RetroPortingToolKit/psxrecomp/pull/559) → [#560](https://github.com/RetroPortingToolKit/psxrecomp/pull/560): PGXP depth buffer, perspective-correct Gouraud, seam expansion (opt-in).
2. [#562](https://github.com/RetroPortingToolKit/psxrecomp/pull/562) → [#563](https://github.com/RetroPortingToolKit/psxrecomp/pull/563) → [#565](https://github.com/RetroPortingToolKit/psxrecomp/pull/565): Smooth motion at any refresh / VRR; reprojected in-between frames; debug A/B keys.
3. [#566](https://github.com/RetroPortingToolKit/psxrecomp/pull/566) → [#567](https://github.com/RetroPortingToolKit/psxrecomp/pull/567): package-relative mod resources; HD pack residency across savestates.
4. [#568](https://github.com/RetroPortingToolKit/psxrecomp/pull/568): render thread draws HD packs in command order.
5. [#576](https://github.com/RetroPortingToolKit/psxrecomp/pull/576): opt-in FXAA + supersample factor.
6. [#578](https://github.com/RetroPortingToolKit/psxrecomp/pull/578) → [#580](https://github.com/RetroPortingToolKit/psxrecomp/pull/580): Smooth motion first in-between timing; dynres counts in-between frames.
7. [#579](https://github.com/RetroPortingToolKit/psxrecomp/pull/579): macOS host sampler include.
8. [#582](https://github.com/RetroPortingToolKit/psxrecomp/pull/582): release checks need only the OpenBIOS backend.

Then R4: re-pin + regen #17, #20 (each regenerates `generated/`).

## Owner decisions

- Anti-aliasing: supersample 1.5, dynres floor = display, FXAA off.
- `guest_cycle_scale = 2`. Reprojection (Smooth motion in-between frames) opt-in.
- HD HUD bundled, on by default, credited to Kuid0us.
- Online: 2-4 players, each on their own full-screen view.
- Rewind off in split screen and in every online session.
- Release model: bundled compiled zips (#18). Version: TBD.

## Blockers

- psxrecomp merges above (owner/upstream review) gate #17 and #20.
- Online battle keeps its experimental switch until psxrecomp reserves link memory per netplay session.
- Release: owner picks the version; never tag/publish without the ask.

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

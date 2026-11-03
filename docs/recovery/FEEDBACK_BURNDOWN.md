# Ridge Racer Type 4 feedback burndown

Updated 2026-11-03. Address campaign scenery culling and steering-camera
discomfort first, then high-resolution rewind. Investigate 8K slowdown without
compromising the working 4K picture. Tomba-style resident loading is the final
stretch goal and starts only after items 1 and 2 pass.

## Scope and branches

- Game changes and this record: [`tetrisgm/RidgeRacerType4Recomp`,
  `recovery/upstream-20261102`](https://github.com/tetrisgm/RidgeRacerType4Recomp/tree/recovery/upstream-20261102).
- Shared engine changes: [`tetrisgm/psxrecomp`,
  `recovery/upstream-20261102`](https://github.com/tetrisgm/psxrecomp/tree/recovery/upstream-20261102).
- Starting game HEAD: `deb37395be48c3aca521b9c4901e849934b34820`.
- Starting engine gitlink: `c1f5cafd4102daa57b7342bec7c432aaca8981ce`.
- Shared UI gitlink: `817c2985d371f8d19d70d9d1d9629425ed6cfd68`.

These are the two recovery branches selected by the owner. Keep shared fixes
on the engine fork, then deliberately update the game's engine pin after
validation. The existing `.gitmodules` URL resolves the frozen baseline from
upstream; configure an explicit fork push remote before engine work and make
the game resolve any new fork-only pin from that fork. No UI work is currently
planned. Do not advance upstream pins or import parked experiments as setup.

Read [COLLABORATOR_HANDOFF.md](COLLABORATOR_HANDOFF.md) and
[README.md](README.md) before implementation. The historical fork architecture
and its feature tables are not the current recovery baseline. This request
creates the work queue; it does not qualify fixes, publish a release, change
default branches or merge upstream PRs.

## Current burndown

| Order | Outcome | Current state | Completion evidence |
| --- | --- | --- | --- |
| 1 | Campaign buildings remain visible at the intended distance | Reported; reproduction pending | Moving before/after coverage at the offending scenery and affected campaign courses |
| 2 | Steering at 21:9 has comfortable camera motion | Reported; reproduction pending | Matched steering samples plus owner comfort check |
| 3 | Rewind preserves high-resolution gameplay presentation | Reported; reproduction pending | Composed images and effective-resolution readings before, during and after rewind |
| 4 | Explain and reduce 8K slowdown where practical | Reported; measurement pending | Matched 4K/8K runtime samples and an evidence-backed candidate decision |
| 5 | Replace eligible CD-paced loads with resident assets and native loading | Stretch; gated by 1 and 2 | Cold/warm load timings and equivalent initialized resources and transitions |

**Resolved outcomes: 0/5. Validated fixes: 0.** The owner's report establishes
the symptoms, not their causes. Keep reproduction, prepared, built, tested
and accepted states separate. Update each row with exact commits, results,
remaining failures and retained evidence when its state changes.

## Baseline and shared acceptance

Use the handoff's verified USA SLUS-00797 disc and
[approved-4k.toml](approved-4k.toml): 2160 preset, effective scale 9 where
supported, 8x anisotropic filtering, full PGXP, maximum detail/draw distance,
HD HUD and adaptive Fit to Window. Dynamic resolution is off in that profile.
The handoff's prior 4K visual approval is not proof of all-course correctness
or a performance measurement. Do not repeat a cleared owner playtest as setup.

For new runtime changes, reuse the supported build, CTest and TCP smoke paths.
The handoff reports 26 tests with the disc present; account for missing or
changed tests rather than silently dropping them. Regenerate through
`tools/regen.sh` when codegen inputs or the engine pin require it; never edit
generated C by hand. Start with two compile jobs and check measured host and
storage headroom before builds or captures.

Use disposable profiles and states made by the same build. Preserve native
memory cards, bindings and explicit saved graphics/mod choices. Fresh packages
must enable adaptive widescreen by default. Unattended runs stay hidden/muted
and use TCP inspection; a requested owner handoff uses a normal window with
normal audio. Keep full PGXP enabled. Save-state backward compatibility is
not required; current-build save/load and rewind correctness are required.

For presentation evidence use `present_shot` and wait for `present_shot_seq`;
use `screenshot_hires` / `screenshot_wide_hires` for geometry detail. Native
VRAM screenshots cannot establish high-resolution output. Record requested
and effective resolution with `video_info`, along with `texture_lod`, actual
window/aspect, mods, camera, course, inputs, host and binary/source identities.
Retain bounded raw comparisons outside Git. Use matched built-game samples;
do not replace runtime qualification with helper tests or more tooling.

## 1 Campaign building culling

**Report:** distant buildings disappear in campaign mode. Draw distance is
a hypothesis. Maximum draw distance is already selected in the approved
profile, so selecting it again is not a demonstrated fix. The exact reported
course, camera and position remain to be identified.

- [ ] Reproduce in a real campaign race and retain a current-build checkpoint
  and short moving route through the disappearance. Record whether the object
  vanishes at screen edges, with distance, at section boundaries or under load.
- [ ] Compare matched 4:3, 16:9 and 21:9/adaptive views and relevant existing
  detail choices. Separate a guest visibility-list/LOD cutoff from frustum
  culling, far-transform precision, primitive limits and renderer rejection.
- [ ] Inspect the existing implementation before adding another distance
  control: `src/mods/r4_max_detail_plugin.c`, `r4_pvs.h`, the widescreen scene
  code, `game.toml` cull/draw-distance sites and `docs/MAX_DETAIL.md`.
- [ ] Implement the demonstrated cause at the owning layer. Reuse the existing
  scanners/tests and add only a regression check that detects the actual defect.
- [ ] Validate moving scenery through the affected campaign courses and bends,
  including extra wide margins, tunnel transitions, mirrors, road/sky seams
  and HUD placement. Derive the coverage from the course data and use prepared
  checkpoints/routes; record each course as passed, failed or unverified.

**Done:** the reproduced buildings stay visible to the intended draw range
without new pops, missing surfaces, ordering/heap faults or reproducible 4K
runtime regression. A sampled course pass does not close untested campaign
coverage. Keep any deliberate content limit explicit.

## 2 Steering camera comfort at widescreen

**Report:** left/right steering causes nauseating camera shake at 21:9. It
may be intentional stock camera motion amplified by the wider view; the
specific source of motion has not been established.

- [ ] Capture a repeatable left/right steering sweep at 4:3 and 21:9 using the
  same speed, course section, car and camera. Distinguish camera roll/sway,
  steering-follow yaw, lookaround input, car movement and presentation jitter.
- [ ] Trace the guest camera builder, Car Effects' existing camera stabilizer,
  and camera/widescreen hooks. `src/mods/r4_camera_look_plugin.c` and
  `r4_camera_look.h` are starting points, not proof that right-stick lookaround
  causes the shake. Distinguish the advertised one-pixel jitter correction
  from intentional steering-induced motion and any interaction between them.
- [ ] Remove the offending steering-induced shake/roll in widescreen by default
  if it can be isolated; otherwise use measured strong attenuation. The owner
  explicitly prefers disabling it to preserving uncomfortable motion. Retain
  useful steering response, camera switching and deliberate lookaround.
- [ ] Check both race cameras, steering reversals, analog and digital controls,
  pause/resume, current-build save/load and adaptive aspect changes. Preserve
  explicit settings; any stock-motion option is an elective enhancement.
- [ ] Prepare one concise owner comfort comparison at 21:9 after automated
  checks pass, and read back the owner's observations before closing the item.

**Done:** the repeatable oscillation is removed or substantially reduced at
21:9, the owner accepts the comfort result, and camera/control behavior remains
correct. Screenshots alone cannot establish motion comfort.

## 3 Rewind without low-resolution gameplay

**Report:** rewind apparently requires or produces low-resolution output.
Determine whether this affects capture, the selected full-size preview, the
restored frame or subsequent gameplay. Small filmstrip thumbnails are a
separate presentation choice.

- [ ] Reproduce at the approved 4K settings and 21:9. Read `video_info` and take
  composed captures before opening rewind, while selecting history, after
  cancel, immediately after restore and after several resumed frames.
- [ ] Separate dynamic-resolution changes from loss of high-resolution/PGXP
  surfaces or texture residency during restore. Inspect upstream's
  `psx.feature.rewind` path, the game's package override and existing
  `tools/rewind_demo.py` / `tools/rewind_soak.py` before adding new machinery.
  These scripts include historical fork command assumptions; verify their
  commands against the recovery engine before reusing them.
- [ ] Preserve or reconstruct the presentation data needed for the selected
  high-resolution view and resumed game. Bound CPU/GPU memory and capture
  cost; keeping an unbounded collection of 4K/8K framebuffers is not acceptable.
- [ ] Check repeated open/cancel/restore, backward selections, timeline
  branching, pause and current-build save/load. Keep the existing two-player
  rewind restriction unless this task demonstrates a reason to change it.
- [ ] Measure enabled-rewind gameplay overhead and restoration latency at 4K;
  check 8K only where effective resolution and host capacity support it.

**Done:** the selected full-size rewind view and resumed gameplay preserve the
chosen effective resolution, aspect, PGXP geometry and texture quality without
stale frames or a persistent low-resolution fallback. No requirement to revive
the historical hold-to-rewind architecture or load old-build states is implied.

## 4 8K performance investigation

**Report:** 8K slowed down on the owner's machine; 4K ran acceptably. This
does not identify a CPU, GPU, transfer, memory or rewind bottleneck.

- [ ] Record the actual host/GPU, output size, requested/effective 4K and 8K
  scale, filtering, rewind settings and identical moving campaign workload.
  Keep internal rendering resolution distinct from physical 8K output.
- [ ] Collect short matched frame-time, guest-progress, CPU/GPU and audio
  samples using existing telemetry. Select a candidate from the demonstrated
  bottleneck, keeping 4K visuals and full PGXP intact.
- [ ] Reuse relevant isolated work from the
  [parked performance assessment](https://github.com/tetrisgm/psxrecomp/blob/review/r4-performance-candidates-20261102/docs/R4_PERFORMANCE_CANDIDATES.md)
  only when it addresses that bottleneck. Qualify its actual built runtime;
  component-test passes are not performance wins.
- [ ] Accept a measured useful improvement or reject the candidate with its
  raw results. Report a hardware-bound limit honestly if full-speed 8K remains
  unavailable. Do not silently lower resolution and call that an 8K fix.

**Done for investigation:** an evidence-backed bottleneck and candidate
decision are recorded. **Fixed** requires verified acceptable 8K runtime on
the reported host. Track those separately; 4K remains the working baseline.
For shared optimizations, follow the central optimization campaign and retain
per-game non-regression evidence before broader promotion. Neutral results are
acceptable; aggregate gains cannot offset a reproducible individual regression.

## 5 Tomba-style resident loading and native initialization

**Stretch gate:** items 1 and 2 must pass first. Begin with measured R4 loads,
not a general CD subsystem rewrite or wholesale replay of disc-prefetch work.

- [ ] Time cold preparation and warm boot-to-menu, campaign race entry, retry
  and race-to-next-menu/next-race transitions. Separate disc reads, decoding,
  resource upload, guest initialization and intentional presentation delays.
- [ ] Study Tomba's current resident preparation in
  [`tomba_seamless_prepare.cpp`](https://github.com/mstan/TombaRecomp/blob/master/src/mods/tomba_seamless_prepare.cpp)
  and native request handling in
  [`tomba_seamless.c`](https://github.com/mstan/TombaRecomp/blob/master/src/mods/tomba_seamless.c).
  Freeze the reference commit used for implementation. Reuse the resident-pack
  framework where appropriate; derive an R4 asset/loader contract from R4 data.
- [ ] Prepare bounded immutable assets from the effective owner-supplied disc,
  with disc/mod identity, cache invalidation and original/derived byte checks.
  Keep mutable initialized game state out of the resident pack. Qualify any
  native decode/initialization against the existing implementation's outputs,
  required RAM/VRAM/SPU effects and observable completion ordering.
- [ ] Preserve real audio, movies, race cadence and resource completion. Keep
  unknown requests and unsupported or incompatible inputs on a working
  reference path rather than returning false success. Qualify supported media
  and mod combinations; explicitly record remaining fallback coverage.
- [ ] Measure cold cost, warm transition gains, RAM, disk cache size and audio
  continuity. Validate repeated retries, campaign transitions, current-build
  save/load and rewind across loads, without changing native player saves.

**Done:** eligible transitions become measurably faster through actual cached
resource delivery/native initialization, with correct game/audio state and
bounded preparation/storage. Retain a buildable LLE reference; qualified HLE
may be selected at build time. No BIOS stubs, fake completion or global game
speedup. Do not ship or expose `psx.enhancement.cd-speed` or
`psx.enhancement.fast-loading`; development catalogs keep them hidden/disabled.
Raw disc read-ahead alone does not establish removal of CD-paced guest waits.

## Next action and reporting

The next implementation action is to reproduce item 1 in campaign mode at
4K/21:9 and resolve the responsible visibility decision. Then address item 2;
avoid spending the first work period on a new harness. Reuse the existing
course checkpoints, debug commands, scanners and tests where available.

For each evaluated candidate record its source/engine/binary identities,
matched settings and host, before/after behavior and costs, raw evidence path,
decision, and remaining acceptance gaps here and in its owning tracker issue.
Stop unproductive tuning after two failed experiments and reassess the cause.
If test capacity is unavailable, prepare the next independent item while
preserving the serial qualification gates. Prepared work is not a passed item.

Current evidence is the owner's report plus the linked recovery and Tomba
sources. No new game build, gameplay test, screenshot, benchmark, disc copy or
runtime fix was produced while creating this burndown.

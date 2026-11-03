# Ridge Racer Type 4 feedback burndown

Updated 2026-11-03. Address campaign scenery culling and steering-camera
discomfort first, then high-resolution rewind. Tomba-style resident loading is
the final stretch goal and starts only after items 1 and 2 pass. The owner
explicitly removed the 8K slowdown investigation; use the working 4K picture
as the validation baseline and do not spend this goal on 8K performance.

## Goal

When invoked through `/goal`, implement and validate the four outcomes below
in `F:/Projects/psxrecomp-ridgeracer`, in the stated order. Progress beyond
planning and diagnosis to working changes, measured candidate decisions and
the prepared owner handoff. Creating this file or returning its command does
not activate the goal.

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
and its feature tables are not the current recovery baseline. Read applicable
AGENTS.md and CLAUDE.md instructions, with the owner's current scope taking
precedence over historical workflows. Work solo. Commit and push validated
changes only to the two selected recovery branches; this owner authorization
supersedes the older recovery instruction against pushing. Preserve concurrent
work and use normal fast-forward pushes, never force-push someone else's work.
Do not publish a release, change default branches or merge upstream PRs.

## Current burndown

| Order | Outcome | Current state | Completion evidence |
| --- | --- | --- | --- |
| 1 | Campaign buildings remain visible at the intended distance | Reported stationary case fixed by `98abdac`; wider course and uncontended performance qualification remain open | Stationary owner-slot replay plus moving coverage and per-course qualification |
| 2 | Steering at 21:9 has comfortable camera motion | Level-horizon candidate passes automated moving checks; owner comfort pending | Matched steering samples plus owner comfort check |
| 3 | Rewind preserves high-resolution gameplay presentation | Accepted for the tested R4 workload; final integrated restore retains HD HUD and scale 9 | Repeated both-camera restores, bounded metadata, neutral sampled cost, current-build save/load and final composed comparison |
| 4 | Replace eligible CD-paced loads with resident assets and native loading | Stretch; gated by 1 and 2 | Cold/warm load timings and equivalent initialized resources and transitions |

**Resolved outcomes: 1/4.** The owner's report establishes
the symptoms, not their causes. Keep reproduction, prepared, built, tested
and accepted states separate. Update each row with exact commits, results,
remaining failures and retained evidence when its state changes.

### Current implementation evidence

The owner supplied the exact stationary reproduction: UI save slot 4 (TCP
slot 3), chase camera, player stopped on the construction straight after
the tunnel. The two distant buildings disappear and return as cars pass.
The source state SHA-256 is
`9d887967a41f73abf328131cc8208474d1bc24eebab67397a1c8606d225577f2`.
The camera is identical in all 50 one-second observations; the scenery list
varies from 24 to 36 blocks. The original repeatedly retracts the buildings
after they have already become visible.

Existing trace output identifies the cause: useful work remains below about
52% of the guest budget, while the heap governor oscillates as a section-cost
jump crosses its narrow memory band. A control using the existing developer
override of ten forward sections retains 36 blocks and both buildings.
Its peak heap use is 101,916 of 135,192 bytes (75.4%), with zero limit hits.
This establishes a governor issue; a larger RAM map is not needed for this case.

The candidate keeps growth below 65% heap use, holds the chosen reach through
80%, reduces above 80%, and retreats two levels at 85%. CPU cutbacks remain
unchanged. A 600-frame regression models the observed ~64% to ~74% section
step plus passing-car fluctuations: the original fails by repeatedly
retracting the skyline; the candidate settles at the newly admitted level.
Threshold rounding, CPU overload and the existing pacing/heap tests pass.
The candidate build and all 26 CTests pass.

Candidate binary
`9008493719bfdd36d935de8109bb8375a3a1972ac433cbb53d6246afa8e671ad`
uses the normal adaptive governor, with no fixed-reach override and 2 MB RAM.
After its initial distance expansion, it holds level 8 / 36 blocks and both
buildings throughout the remaining stationary sample; the original retracts
them repeatedly. Exact input/configuration, images and traces are retained
under ignored `analysis/feedback/stationary-pop/`. The source owner slot is
preserved; diagnostic copies use the same frozen state and an isolated profile.
Implementation is `98abdac9473a3616beae0c4654763fddd5a618ca` on the selected
game recovery branch; the engine pin remains `b9dcf919`. The short full-grid
drive retains effective scale 9 and zero dispatch/segment misses. Its sampled
heap peak is 111,372 bytes (82.38%), with zero limit hits; inspected road,
mirror and tunnel surfaces remain intact.

Two 20-second stationary samples per build use the same frozen slot, native
overlay cache and 4K/21:9 settings, with tracing disabled and no screenshots
during timing. The original owner window is host-paused during each sample
and then restored. Host CPU pressure is high and differs between runs.

| Build | VBlanks/s | CPU ms per game tick | Host CPU % |
| --- | --- | --- | --- |
| Original `296672e7…` | 33.52 / 32.85 | 81.05 / 84.16 | 92.0 / 95.3 |
| Fix `90084937…` | 34.09 / 35.92 | 92.43 / 89.24 | 88.6 / 84.7 |

Frame-rate differences are inconclusive under this changing contention.
Observed CPU work per game tick is about 10% higher with the additional
geometry continuously present. This is a correctness result with a measured
resource cost, not a performance-win or locked-60 claim. Uncontended 4K and
full campaign qualification remain open. Raw samples are each profile's
`stationary-cost-trace0.json`. The source owner slot retains its original hash.

The owner identified the default first campaign race, Helter Skelter. The
draw-distance governor counted its own pacing wait as rendering work. The new
actual-plugin regression fails against the original and passes with sampling
before that wait. All 26 supported CTests pass, including actual PGXP boot.
New 48-second moving samples retain effective 2160/9 at 21:9 with zero dispatch
and segment misses. Sampled road, tunnel and scenery surfaces remain intact.
The later stationary owner slot above reproduces the reported pop; complete
course coverage remains open.
Retail primitive-heap pressure can independently reduce distance. An isolated
trial of the existing 8 MB RAM enhancement enables R4's existing larger heaps
and holds distance level 30. This broad candidate is not promoted: it does not
establish neutral 4K runtime cost and does not isolate the reported object.
No RAM-default change is included. The later owner-provided slot above
supersedes the earlier incomplete scenery-pop diagnosis.

A follow-up hidden run on the final integrated binary compares the same
current-build grid checkpoint at 4:3, 16:9 and 21:9. Fixed Cross input is
sampled every 120 VBlanks for the first 600 (about ten seconds), with actual
composed-present fences, RAM poses and visibility lists retained. The paired
towers and distant overpass on this first straight remain visible in all
three aspects. Positions differ by 0–3 game ticks, so the comparisons are
near-matched rather than pixel-identical. All samples retain scale 9, with
zero dispatch/segment misses. This is partial first-straight coverage and
does not reproduce or close the reported building pop, the bends/tunnels or
other courses. Evidence: `analysis/feedback/culling-aspects/`.

Moving follow-up runs retain 25 native, 24 16:9 and 25 21:9 composed samples
over roughly 48 wall-clock seconds per aspect from the same checkpoint.
The inspected first bends, tunnel walls/exit and following city surfaces are
intact. All runs retain scale 9 with zero dispatch/segment misses; widened
views retain zero roll. Their poses differ with the closed-loop driver and
host pacing, so these are nearby-section comparisons, not exact-frame pairs
or performance measurements. Those route samples did not reproduce the
disappearance; the later stationary owner slot above does.
Raw poses/images are `culling-aspects/route-{800,1067,1400}/`.

Course coverage is derived from the game's eight 24-byte titles at
`0x800A19E8`; title renderers at `0x8005A430`/`0x8005A8A4` read the signed
course index at `0x800F3AC4`. The default campaign checkpoint has index 0.
This identifies the coverage below; reading the table does not qualify a
course's rendering or resource initialization.

| Type | Course | Scenery qualification |
| --- | --- | --- |
| 0 | Helter Skelter | Owner slot-4 pop fixed; straight/bends/tunnel/city sampled; full course remains open |
| 1 | Out of Blue | Unverified |
| 2 | Edge of the Earth | Unverified |
| 3 | Brightest Nite | Unverified |
| 4 | Wonderhill | Unverified |
| 5 | Heaven and Hell | Unverified |
| 6 | Phantomile | Unverified |
| 7 | Shooting Hoops | Unverified |

The camera candidate clears only the transient race-view roll when the view
is wider than 4:3. All 24 moving wide samples have roll zero; the baseline has
both signs, up to 19 units. A native-aspect run retains stock roll (-17 to 13
over 163 samples). Steering, yaw and deliberate look remain active. Actual
owner comfort and the combined handoff remain required.

Rewind retains effective scale 9, but the original loses HD HUD upload
provenance after restore. The framework candidate retains the existing
pack/VRAM-validated residency metadata per point and restores it after the
frontend restage. Eight actual cockpit restores and four chase restores retain
the HD glyphs; the chase camera was switched with actual Triangle input and
verified. Current-build slot 9 save/load passes. Fifty points use 1,499,200 bytes
of metadata, capped at 32 MiB by retiring oldest complete points. Rejected
loads, branching, duplicate ticks, depth/budget eviction, discard and shutdown
pass the existing actual-ring context test. The no-rbengine path compiles and
the real GL HD-pack fixture passes its pack/VRAM guards. Engine fix `b9dcf919`
is pushed to the selected fork branch. The game pin and fork URL are updated.
Supported regeneration emits 52 unchanged shards and an unchanged dispatch
table; the integrated binary is
`296672e7ff72267fe6db7d6eaa7b4dbc45d21690c07260cfa7d4cbf0ee754b22`.
Its build and all 26 CTests pass. Three timed restores take 62–78 ms through
the host input/TCP path, returning to demonstrably earlier guest ticks while
retaining effective scale 9. The integrated binary also passes both cameras
with digital left/right and analog left/right at 4:3, 16:9 and 21:9. Every
wide sample has roll zero and retains nonzero steering; native samples retain
stock roll. Actual composed presents fence each adaptive resize. Pause/resume,
current-build save/load and rewind open/cancel pass, with PGXP and scale 9
retained, native overlays loaded and zero dispatch/segment misses.
Owner review and complete course coverage remain open. The first combined
attempt did not fence hidden-window resize presentation, and the next exposed
an input-test release-edge timing error; neither is counted as a game defect
or an accepted native-aspect sample. The corrected samples and the affected
pause/resume recheck are retained in `combined-review/combined-checks.json`.
The final integrated rewind smoke captures before/open/selection/restore/resume
at effective scale 9, with 1,499,200 metadata bytes for 50 points. The static
HD RANK glyph mask is identical before/after (0 changed mask pixels), and
the composed restored frame was visually inspected. This closes item 3 for
the tested R4 workload; it is not a claim of other-game or all-course coverage.
Implementation is game commit `fddc0e2` with engine `b9dcf919`.

Fixed-input runs on DESKTOP-HN15M1H use the same compiled native-overlay cache,
4K/effective 9, 21:9, PGXP, HD HUD and approved detail settings. There is no TCP
polling during each 20-second sample. These initial runs have diagnostic
`R4_MD_TRACE=2` enabled in both arms. Each configuration uses its own
current-build state and two samples; this is a focused decision, not an
all-course performance qualification.

| Configuration | Binary SHA-256 | VBlanks/s | Game ticks/s | Process CPU seconds per 20 s | Host CPU % |
| --- | --- | --- | --- | --- | --- |
| Before rewind fix, retail heap | `6432e1747de8f33cc4d97a9a8159d3f0aa0a836ccd599dfada84bae6ec13c272` | 59.95 / 59.30 | 29.98 / 29.65 | 26.66 / 29.28 | 48.0 / 68.7 |
| Rewind fix, retail heap | `5639a5a00f4e7f5b8f0ccc60dfe4cdc4f854d35b6bd62727879588b8ff671a34` | 60.00 / 59.40 | 30.05 / 29.70 | 27.69 / 29.02 | 49.6 / 55.3 |
| Rewind fix, expanded heap | same candidate binary | 54.40 / 55.71 | 27.20 / 27.88 | 33.59 / 32.17 | 54.8 / 54.9 |

The rewind candidate is neutral within observed variation, with bounded
metadata and no demonstrated gameplay-rate regression. The expanded heap
falls below the working cadence in both traced samples. A follow-up with
tracing disabled gives 56.85/56.86 VBlanks/s for the retail heap (host CPU
61.8/64.8%) and 40.32/42.40 for the expanded heap (83.3/77.6%). The changing
host pressure makes that follow-up's wall-time difference inconclusive; it
does not establish a release-path regression or a neutral result. Stop this
broad all-geometry expansion rather than tuning it further before reproducing
the specific building. Earlier samples at 86–89% host CPU and TCP-driven
autopilot timings are diagnostic only. Raw samples remain in each profile's
`fixed-input-results.json` and `fixed-input-no-trace.json`; the second traced
expanded-heap run supersedes its first.

Raw identities, captures, settings and results are under ignored
`analysis/feedback/{baseline,detail-candidate,camera-candidate,rewind-candidate,heap-candidate}`.
The first unprofiled launch and an initial guest-layer rewind accept attempt
were rejected as setup evidence. Raw moving samples do not establish
all-course acceptance.

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

- [x] Reproduce in a real campaign race and retain the owner's checkpoint
  and stationary observation through the disappearance (the owner's later
  slot-4 clarification supersedes the moving-player-route assumption). Record whether the object
  vanishes at screen edges, with distance, at section boundaries or under load.
- [ ] Compare matched 4:3, 16:9 and 21:9/adaptive views and relevant existing
  detail choices. Separate a guest visibility-list/LOD cutoff from frustum
  culling, far-transform precision, primitive limits and renderer rejection.
- [x] Inspect the existing implementation before adding another distance
  control: `src/mods/r4_max_detail_plugin.c`, `r4_pvs.h`, the widescreen scene
  code, `game.toml` cull/draw-distance sites and `docs/MAX_DETAIL.md`.
- [x] Implement the demonstrated cause at the owning layer. Reuse the existing
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
  cost; keeping an unbounded collection of high-resolution framebuffers is
  not acceptable.
- [ ] Check repeated open/cancel/restore, backward selections, timeline
  branching, pause and current-build save/load. Keep the existing two-player
  rewind restriction unless this task demonstrates a reason to change it.
- [ ] Measure enabled-rewind gameplay overhead and restoration latency at 4K.

**Done:** the selected full-size rewind view and resumed gameplay preserve the
chosen effective resolution, aspect, PGXP geometry and texture quality without
stale frames or a persistent low-resolution fallback. No requirement to revive
the historical hold-to-rewind architecture or load old-build states is implied.

## 4 Tomba-style resident loading and native initialization

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
Follow the central optimization campaign for engine optimization work and
retain per-game non-regression evidence before broader promotion. Neutral
results are acceptable; aggregate gains cannot offset an individual regression.
The separately parked performance branches are references, not dependencies
or accepted implementations. Reuse only a candidate needed for this scope.

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

## Execution and handoff conditions

1. Inspect the current worktree and remote heads before changes. Initialize
   required submodules at the recorded pins, preserve existing work, and use
   the fork's selected engine branch for shared changes. Locate the verified
   local disc and prior useful evidence through known project paths and the
   handoff; never crawl drives or unrelated project roots.
2. Search and claim the owning central Beads issue before engineering; reuse
   the existing R4 hierarchy and relate engine optimizations to the optimization
   campaign. Consult the private pending tracker note at
   `C:/Users/Matthew/.codex/memories/r4-feedback-burndown-tracker-pending.md`.
   Do not duplicate historical performance work or force database locks.
3. Reproduce and resolve culling, then camera comfort, then rewind. Once items
   1 and 2 pass, attempt the loading stretch goal using measured R4 loader
   contracts. Continue useful independent preparation while test capacity or
   owner observations are pending; do not count that preparation as validation.
4. Run the supported build/tests and meaningful current-build gameplay,
   save/load, rewind and changed-feature checks. Measure runtime costs against
   matched baselines. Fix objective failures before calling a change ready;
   distinguish identical baseline failures from introduced regressions.
5. Prepare a concise, reproducible camera-comfort/changed-visual handoff with
   expected results and locally saved owner observations. Preserve earlier
   cleared checks. Do not open or interrupt a visible game without a requested
   owner handoff; use a normal window and normal audio for that handoff.
6. Push validated engine changes to the selected engine recovery branch, then
   update and validate the game gitlink and fork URL as needed. Record pin
   changes in `docs/framework_pin_history.md`. Push the corresponding game
   changes and this updated burndown to its selected recovery branch.
7. Finish with actual accepted/rejected candidates, measured behavior, remaining
   failures or unverified checks, exact commits and evidence locations. Retain
   only deliverables and necessary ongoing evidence; clean owned disposable
   data safely, preserving saves, settings and unrelated files.

Complete the goal only when the required outcomes pass their acceptance checks
and the loading stretch has either passed or received a concrete, evidence-backed
disposition. A rejected loading experiment is not a fixed loading item. If an
indispensable local asset, owner judgment or external service prevents further
progress, preserve a ready-to-resume handoff with the exact missing prerequisite,
continue independent authorized work where possible, and keep affected outcomes
open. Do not stop merely because a build is slow or test capacity is occupied.

The burndown was initially created without runtime changes. Implementation
evidence above supersedes that initial planning-only state; unchecked criteria
and owner observations remain open. Central tracking uses `beads-r4recomp.4`
and framework HD-residency bug `beads-eio.3.433`.

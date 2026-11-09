# R4 history audit and finalization decisions — 2026-11-09

The right integration is the upstream-based recovery **plus the original custom
R4 menu**, with its controls and settings connected to the recovered engine.
Keep MStan's recovery and correctness repairs, the recovered Steam Controller
handling and native overlay packaging. Do not merge the old renderer fork back
as a prerequisite for restoring the menu.

This supersedes the menu decision in `f9643421` and the claim that the earlier
branch cleanup constituted a complete feature audit. That commit supplied a
working generic menu, but it did not restore the owner's custom menu or its
in-game binding/device/setup workflow. Archiving branches preserved source;
it did not establish that every useful behavior was retained.

## Scope and evidence

Baseline examined: game `f9643421`, engine `ae3eae8f`, upstream UI `817c2985`.
Provisional restoration edits are separate, uncommitted and not qualified.
The local build directory has been rebuilt with those provisional edits; it
must not be described as a reproduction of the committed package.

The [machine-readable scope receipt](HISTORY_AUDIT_20261109.json) records:

- 979 distinct reachable game commits including merges across the live history
  and the locally restored archive; 602 from live refs and 377 additional
  archive commits. This is an inventory, not 979 individually verified patches.
- Every historical game gitlink: 252 distinct engine pins and 41 UI pins.
  After exact-object retrieval attempts, 29 engine pins and three UI pins remain
  unavailable in these repositories. They are listed explicitly in the receipt.
- Changed implementation paths for all 70 distinct archived topic tips with
  commits outside the pre-cleanup baseline, compared with current file blobs.
  Equal blobs establish source retention; different blobs do not by themselves
  establish loss. Forty-eight historical implementation paths are absent from
  committed master, including renamed/replaced implementations and research tools.
- Detailed source comparison of the original integrated game (`14d79819`),
  MStan recovery (`d72d212c` onward), PC rescue (`87fc0234`), native packaging
  (`7b573206`), recent menu/controller consolidation (`f9643421`), custom UI
  `0b89a344`, and the unmerged live-graphics work (`6c1a040f` / UI `6bca215f`).

Local indices, topic/file comparisons and provisional patches are under
`analysis/history-audit-20261109/`; their hashes are in the receipt. Source
preservation is `/Users/shokunin/dev-archives/r4-20261109/`. No NAS or Windows
machine was accessed for this audit.

Historical runtime evidence below is attributed to its recorded build and
conditions. The ignored `analysis/recovery/` and `analysis/feedback/` raw trees
were not found in this checkout or the top-level archived game worktrees.
Their committed summaries are available; their pixels and raw timing traces
have **not** been independently rechecked here. Missing old pins and missing
raw traces prevent a literal claim that all historical work was fully verified.
They do not erase the available source or justify importing an old branch whole.

## What happened

| Period / anchor | Actual change | Consequence |
|---|---|---|
| September–October game enhancements | Native-wide races, PGXP, resolution, detail, controls, Link Battle, HUD and optional effects accumulated in game-owned modules and framework APIs. | Most game modules survive unchanged from the last original integrated build. Older branches often contain earlier versions of already-landed work. |
| October 24 upstream Graphics migration (`7a786f8e`, `4734f693`) | Shared engine moved enhancement controls into host-owned mod schemas and removed preset fields. | The old menu needed an API/settings migration. This was not a reason to abandon its design. |
| October 24–November 2 original fork (`84d6ca22`) | Native VRAM ownership, latest-wins packets, retained surfaces, interpolation, audio/rewind and scheduling changed together. | Some optimizations helped; packet gaps, object identity and surface ownership introduced visible defects. This was much broader than menu work. |
| November 2 MStan recovery (`d72d212c`, engine `c1f5cafd`) | Regenerated with native CPU spans; adopted upstream rendering/settings; removed fork interpolation; preserved game enhancements; repaired race-state publication. | Better measured whole-package cadence, but custom screens and hold-to-rewind explicitly not recovered. |
| November 3 MStan follow-up | `98abdac9` scenery governor; `fddc0e2f` wide camera and HD rewind; `568eee52` RPM coverage. | Focused correctness improvements with bounded evidence. These are part of the work to retain. |
| November 5 work, landed November 7 (`87fc0234`) | Max Detail requires expanded RAM; XA-only music no longer treated as movie rewind load. | Addresses diagnosed heap corruption and rewind density. The 4K slowdown report explicitly remains unresolved. |
| November 9 (`7b573206`, `f9643421`) | Native overlay variants and portable cross-build; recovered Steam input policies; generic menu substitution. | Packaging and input work are useful. The menu substitution does not satisfy the owner's UI requirement. |

Git attributes the recovery and focused follow-ups above to Matthew Stanley.
Authorship alone does not identify who measured every historical result.
The original issue #106 contains an AI-generated handoff and a tetrisgm comment;
it should not be presented as independent MStan validation of the entire fork.

## Decisions by workstream

| Workstream | Decision and source basis | Remaining qualification |
|---|---|---|
| MStan/upstream execution performance | **Keep.** `2f2f612e` defaults qualified native spans and ordered GL batching; recovery regenerates the game for them. Current configured build has native spans ON, native OpenBIOS reads ON and GL batch implementation HLE. | No PGO performance claim: current `PSX_PGO` is empty. Do not attribute all recovery gain to native spans. |
| Small old-fork texture optimizations | **Keep the existing upstream equivalents.** Current `duckstation_texture_pack.cpp` contains the linear residency set from `008d3d4a` and the intersect/released guards from `7bc8663e`. | No duplicate cherry-pick or new speed claim needed. |
| Packet/native-VRAM/interpolation redesign | **Keep archived; exclude from finalization baseline.** See `FRAME_INTERPOLATION_STATUS.md`, issue #106 and engine `734794d5`, `8d14308b`, `91b54cd2`. Corrective commits depend on that architecture. | Native cadence remains the baseline. Reviving smooth motion requires independent endpoint correctness, identity/cut/restore tests and distinct-frame performance evidence, not just swap counts. |
| Original custom menu | **Restore and adapt.** UI `b31c370`, `5d8eaa3`, `df6df24`, `0b89a344` already implement boot/main screen, Select tap, Options, states and Start/Resume focus. | Verify actual overlay appearance and all menu actions over the recovered engine. No special chord required. |
| Latest Graphics page | **Recover its intent, rework its binding.** `6c1a040f`/`6bca215f` expose named image/PGXP/title rows but were marked WIP and unverified. | Use current host mod schemas and actual live/restart behavior. No resurrected quality presets, dead settings or selectable PGXP-off state. |
| Steam Controller | **Keep adapted fixes** in `fb34f318`: keyboard-seat adoption, physical Valve echo guard, virtual-pad preference, Steam HIDAPI hint. Preserve current retry/hotplug and fast-forward handling. | Physical direct launch and Steam Input remain owner tests; synthetic event tests do not establish hardware behavior. |
| Modern/Classic controls, camera look, JogCon/FFB | **Keep.** Their game sources are unchanged from `14d79819`; live scheme work `4a376192` and response curves `cc68f7f4` survive. | Restore menu binding/device editing. Test live effects, persistence, reconnect and no double input; physical wheel/rumble and manual shifting are not established by unit checks. |
| Widescreen and camera comfort | **Keep recovered version.** Original visibility/HUD hooks remain; `fddc0e2f` clears transient camera roll only in wide race views. | Recorded moving wide/native samples support behavior, not all tracks or owner comfort. Preserve native-aspect roll. |
| Max Detail stationary pop | **Keep `98abdac9`.** Source samples useful work before pacing and uses heap hysteresis: grow below 65%, hold through 80%, retreat above it. | Owner slot result is recorded as accepted. More continuously visible geometry cost CPU; this is not a speed optimization. |
| Max Detail crash protection | **Keep expanded-heap requirement from `87fc0234`.** Existing plugin relocates two heaps to 1 MiB regions; recorded corruption exceeded retail heap and damaged executable RAM. | Full normal-paced rank-8 reproduction still required. Do not remove protection to improve timing. |
| 4K crowded-race slowdown | **Unresolved; investigate separately.** Expanded heaps let the guest-budget governor reach level 30/32 ahead sections while heap use is only 5–8%; host GPU cost is not its input. | Measure matched crowded sections at owner quality. Optimize proven redundant work; do not quietly reduce resolution, detail, PGXP or override `present_thread=false`. |
| HD HUD / RPM | **Keep.** Existing pack-based replacement survives; `3a0502c5` fixes replacement alpha coverage without changing guest VRAM or artwork. | Recorded real GL coverage checks and scene tests are bounded. Other seven course maps and all modes are not proven. Old custom UI atlas experiments are not the active HD HUD implementation. |
| Rewind and save/load | **Keep upstream filmstrip plus `b9dcf919` HD residency and `006fb5bb` XA fix. Restore custom menu save/load controls separately.** | Hold-to-rewind was deliberately not recovered and is not equivalent to filmstrip. Its old enhanced replay depends on packet history; do not reintroduce that architecture just for UX. |
| Car Effects and race-state sampler | **Keep current safe publication and guest-clock behavior.** Recovery replaces unsafe concurrent struct copies with a lock and removes packet-metadata dependence; later ImGui compatibility is small. | Individual triggers and moving presentation alignment remain unqualified. Existing source alone does not prove correct placement on the current presented frame. |
| Local VS and online Link Battle | **Keep existing game implementation.** Source and supporting game configuration survive the recovery; upstream local-view APIs remain the integration basis. | Full 2–4 peer, per-seat view, rematch and UI setup tests are not covered by single-player smoke. Don't label netplay finished. |
| Rear-view mirror option | **Keep persisted user choice.** The existing game module is unchanged by recovery; its separate hide option and Max Detail mirror setting need understandable UI. | Do not silently hide the mirror as a performance fix. Test both options together when qualifying settings. |
| Native overlays / packaging | **Keep `7b573206`.** 7,459 guarded variants cover nine unique overlay images; tested route has native dispatch and no misses. Portable package excludes disc/EXE data. | `full_static_coverage_proven:false` remains honest. Packaging success does not establish all game modes or correct custom menu integration. Rebuild after restoration. |
| Idle skip, PGXP emitter, snapshot and release-wait research | **Preserve as research leads** (#108/#109), not automatic cherry-picks. Historical branch content includes pin/config experiments, generated changes and architecture-specific instrumentation. | Re-establish a current hot path and semantic equivalence before adopting. Never hand-apply archived generated-code edits. |
| Resident loading | **Defer with explicit status.** Recovery burndown records an uncompleted stretch goal, gated on core visual work. Upstream infrastructure is not proof that R4 loaders were converted. | Measure R4 load phases and initialized-resource equivalence before implementing title-specific acceleration. |
| Old verification tools | **Separate historical tools from usable current checks.** `tools/perf/measure.py` assumes `packet_cpu` and `packet_interpolation` from the removed pipeline. Old FPS/atlas/audit harnesses also target retired implementations. | Do not run these and interpret missing counters as zero or claim their presence is retained functionality. Port a relevant measurement before using it for acceptance. |

## What the performance evidence actually supports

MStan's committed [validation](validation.json) compares two 20-second runs per
whole package at 1080p/effective scale 5, PGXP on, original race cadence and
Classic input for automation. Original: 54.64 / 52.53 VBlanks/s; recovery:
59.98 / 58.59. Process CPU time also falls. However, checkpoints were prepared
independently, runs were ordered, rewind implementations differed, and one
recovery arm still reported audio starvation. This supports the recovery
choice; it does not isolate a single optimization or prove 4K performance.

The October 30 release receipt reports 59.994 presentation completions/s over
120 seconds at scale 18 with dummy audio. It explicitly does not count unique
pixels or physical scanout. Earlier interpolation failures and later successful
wait-parity tests concern different gates. Neither should be erased, and neither
licenses claiming all visual defects were closed. November 2 reports still
record dropped packets, flashes, identity heuristics and unbisected seams.

The [November 5 report](PERFORMANCE_HANDOFF_20261105.md) records a normal-paced
35-second visible 4K run averaging 57.8 VBlanks/s with six seconds below 55.
Scene GPU cost averaged 32.37 ms in a recent ring, but those GPU and VBlank
samples were not synchronized. GPU workload is a supported hypothesis, not a
proven per-frame cause. Wrong-port and low-power-state ablations were rejected;
they must not become evidence for lowering quality.

## Menu restoration contract and dependency analysis

The custom menu is independently recoverable from the renderer fork. Its host
boundary is `PsxMenuHost` plus the launcher settings/mod providers. Restore the
R4 skin and main/Options flow; use Select/Back alone to open it, with ordinary
back/resume behavior. Preserve the custom controls and settings pages.

Source inspection found the following migration work, beyond simply compiling:

1. **Controls:** current `mh_launcher_settings_get` fills player source but
   hardcodes analog mode and omits actual device/deadzone data. Its apply path
   handles video, volume and FFB, but not those seat fields. Bind-file reload is
   a separate path. Editing devices must update runtime state and persisted
   settings, with the same seat/adoption policy as the pre-game launcher.
2. **Graphics:** current engine hides mod-owned legacy rows. Old title source
   hides `graphics.title_features`, and old direct preset/resolution fields do
   not map automatically to current mod schemas. Restore named rows through
   actual providers; verify readback, persistence and restart indications.
3. **States:** old `cc3a08b4` adds the menu-host slot API. A provisional port is
   saved but untested. Apply save/load at the host's safe point and respect
   netplay/save-policy restrictions, thumbnail lifetime and failure feedback.
4. **Disc and artwork:** current in-game GameInfo lacks old disc-verification
   wiring; the custom session's single-disc path discovery can miss a directly
   mounted disc. Provide current mounted-disc identity and ownership-safe
   callbacks so the menu does not falsely request setup or lose its art.
5. **ABI and lifecycle:** the custom header is older. Three compatibility
   additions allowed an initial Mac build, which establishes compilation only.
   Pause/resume, render ownership, restart, session transitions and input capture
   still need runtime checks. The earlier black-menu fix `a44c5467` demonstrates
   why a menu that compiles can still fail while the guest is paused.
6. **Steam:** retain the current input fixes in ordinary play and paused-menu
   event processing. Add-to-Steam UI and physical Steam Controller support are
   distinct features; one passing does not establish the other.

## Execution order and completion evidence

1. Restore the custom menu on the retained engine, adapting the boundaries above.
   Replace the generic-menu-only smoke expectations with actual custom menu
   checks. Keep raw restoration patches until the integration is qualified.
2. Verify startup, Select open, Back/resume, pause stability, controls rebinding
   and devices, graphics readback/persistence/restart, volume, save/load and
   missing/valid-disc flows. Check the actual rendered custom UI, not only APIs.
3. Re-run Mac Release, relevant CTests, PGXP-on isolated smoke and native-overlay
   dispatch checks. Regenerate supported outputs for pin changes. Preserve
   native saves and settings; hidden/dummy automation only.
4. Cross-build a new portable Windows candidate with exact source/pin identity.
   The existing `2026-11-09-consolidated-01` contains the generic menu and must
   not be relabeled as the restored-menu candidate.
5. Qualify the retained gameplay risks separately: crowded 4K race, rank-8 route,
   effects alignment, camera comfort, course coverage and online modes. Hardware
   controller/audio/Windows acceptance belongs to owner testing, not Mac smoke.

The audit establishes the integration direction and exposes the incomplete
contracts. It does **not** establish a finished restored menu, solved slowdown,
all-course correctness, or verification of unavailable historical objects.

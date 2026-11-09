# Finalization baseline — 2026-11-09

Development is on game `master` in the Mac checkout. There is one local and
one remote game branch, and one registered game worktree. No NAS or Windows
machine participates in development or delivery.

## Decisions from the body of work

| Work | Disposition |
|---|---|
| MStan/Game Master performance recovery | Keep the upstream-based native CPU spans and rendering baseline. The prior recovery and PC-rescued work are already in master. |
| Later packet, interpolation, native-VRAM and pooled-renderer experiments | Preserve as research; do not merge the old engine fork back over the recovered engine. Issues #106, #108 and #109 retain the unresolved leads. Smooth motion stays off; PGXP stays on. |
| Maximum detail / crash recovery | Keep the recovered detail work and expanded-memory path. Mac smoke is not proof that the normal-paced rank-8 hang or 4K slowdown is resolved on Windows. |
| Original integrated menu | Its title source survived recovery, but was not compiled by upstream recomp-ui; the upstream default provider declined this single-disc title. Replace the inactive title layer with an explicitly registered R4 menu using upstream's shared model and renderer. |
| Live Graphics experiments | Use actual host-owned mod schemas and live/restart results. Do not restore removed presets or promote the unverified old live-graphics branch wholesale. |
| Steam Controller | Recover keyboard-seat adoption, physical Valve desktop-echo suppression, virtual-pad preference and the SDL Steam HIDAPI driver. Keep the newer hotplug/retry and hold/toggle logic. Physical Steam Controller/Steam Input qualification remains an owner test. |
| Native overlays | Keep all 7,459 guarded variants from nine original-disc images, built directly into Mac and Windows executables. No runtime overlay compiler/cache. |

## In-game UI

Esc, F10, Guide, or Start+Back opens the menu. Arrows/D-pad navigate;
Enter/A selects and Esc/B goes back. Mouse navigation is supported. The game
pauses offline and resumes without a session restart. Online does not pause.

Display, session volume and the host's installed mod settings are available
over the game. This includes Internal Resolution, image filtering, R4 control
scheme, camera and detail options exposed by their manifests. Live settings
apply through the engine; restart-only changes are saved and identified as
such. PGXP's enable switch is locked on. No quality preset is introduced.

Disc setup, full binding editors, memory-card management, online setup and
unsupported text/64-bit mod fields remain in the full launcher. Selecting
**Full launcher** ends the session; save progress first. This is a maintained
settings surface, not a claim of visual/feature parity with the retired custom
launcher. The old implementation remains recoverable from Git history.

## Recoverability and branch cleanup

The [exact ref inventory and receipt](archive/CONSOLIDATION_20261109.json)
records all 62 retired remote topics, their tips and commits absent from master.
Every local and remote tip was checked against a self-contained Git bundle
before deletion. The bundle was also restored to a mirror and checked with
`git fsck --full`. There were no open game PRs when topics were retired.

Local preservation is `/Users/shokunin/dev-archives/r4-20261109/`:

- `game.bundle`, `verified.git`, branch inventory and remote deletion receipt.
- `worktrees/`: all 44 old working directories, moved intact, including ignored
  files and unfinished generated-code edits. This is preservation, not disk-space reclamation.
- `unused-ui-third-party/`: the unused nested ImGui clone that previously
  made the UI submodule dirty. The build uses `recomp-ui/src/third_party/imgui`.
- `worktree-metadata/`: original Git worktree metadata, plus binary patches.
  Archived directories are not active checkouts; their old `.git` links are
  intentionally not repaired into new development worktrees.

To recover a topic, clone `game.bundle` into a separate archival directory and
check out the recorded commit. Read the archived working directory or apply its
saved binary patch for uncommitted work. The live checkout continues on master.

The earlier 36 backup/archive/recovery deletions and public-EXE preservation
remain documented in the historical receipt. No additional NAS access was
needed for this cleanup. Engine PR branches remain review artifacts until
upstream accepts their fixes; they are not parallel game development lines.

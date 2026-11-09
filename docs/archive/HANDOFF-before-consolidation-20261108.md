# Handoff

## Current (2026-11-07): PC rescue reintegrated

The Windows PC was compromised (Nov 4–5) and is being wiped. All R4 and
psxrecomp work from it was pushed as `backup/pc-20261106/*` branches (keep
them); the full worktrees, including ignored analysis, are on the NAS at
`/volume3/pcbackup/pc-20261106/work/`.

**Master is now the recovery line.** `recovery/upstream-20261102` (current
upstream psxrecomp/recomp-ui, game-owned enhancements ported, fork packet
interpolation omitted; owner reviewed 4K/8x on 2026-11-03) was merged into
master, with the PC's uncommitted 2026-11-05 work on top:

- Max Detail requires the bundled 8 MB RAM feature, so its plugin moves the
  primitive heaps out of retail RAM. Fixes the owner's rank-8 race hang
  (heap overflow overwrote code at `0x800600D3`).
- psxrecomp `006fb5bb` (`tetrisgm/psxrecomp` `fix/rewind-xa-not-movie`, also
  the tip of `recovery/upstream-20261102`): rewind stops densifying snapshots
  while only XA music streams.
- `docs/recovery/README.md` owner race-validation notes and
  `docs/recovery/PERFORMANCE_HANDOFF_20261105.md`.

Read `docs/recovery/README.md` first. The previous fork-architecture handoff
is archived in `docs/archive/HANDOFF-fork-20261030.md`; the pre-recovery game
head is `archive/original-20261102`.

Verified on the Mac only: Release build and CTest (see the commit). Not
verified: Windows build, smoke, the owner's crash route under normal pacing,
and any on-screen check — these wait for the rebuilt PC.

## Parked (on GitHub, not on master)

| Branch | Content | Why parked |
|---|---|---|
| R4 `backup/.../ridgeracertype4/fix/race-real-time`, `pc-wip/ridgeracertype4` | Frame-architecture phase 3 docs/evidence, `CODEX_PERF_PROMPT.md`, pin bumps | Fork architecture the recovery replaced |
| R4 `pc-wip/r4-wt-pgxp-audit`, `pc-wip-tracked/r4-wt-release-lap` | Fork packet interpolation + PGXP audit + release harness | Superseded; Car Effects guest-clock fixes already ported in recovery |
| R4 `pc-wip/r4-wt-final`, `pc-wip/r4-wt-interp`, `launcher-xnet-*` | Pin bumps only / empty / Oct 8 netplay launcher surface | Superseded by later master and recovery |
| R4 `pc-wip/perf-lowend-20261008__r4` | `idle_skip` + graphics presets in `game.toml` (old schema) | Lead for the slowdown; upstream has its own idle-wait support, needs re-port and A/B |
| psxrecomp backups (22 branches, both orgs) | Fork `r4/main` topics, frame-arch 1–3, threaded rendering, pooled restage | All but the rewind fix belong to the superseded fork; `int/threaded-rendering` (52 commits) and `fix/queued-vram-snapshot` (2) are unique but fork-based |
| recomp-launcher `r4/main` | Original custom R4 launcher (`0b89a344`) | Recovery uses shared recomp-ui `817c2985`; custom R4 screens not ported |

## Owner decisions needed

- `CLAUDE.md`'s "Recovery branch override" (merged from the recovery branch)
  still says "do not publish, push or merge" and supersedes the fork workflow.
  Confirm the new rules for master.
- The backup branch `pc-wip/r4-mstan-validation` commits
  `desktop-r4-archive/SLUS_007.97` (the game EXE) to the public repo. Removing
  it needs a force-push/delete of a backup branch; the files are also on the NAS.

## Next

1. Rebuild the PC (or another Windows box): build, CTest, `tools/smoke.py`,
   replay the owner's rank-8 crash route under normal pacing.
2. Race slowdown (`PERFORMANCE_HANDOFF_20261105.md`): GPU scene time near the
   30 Hz budget at 4K/Max Detail; keep the owner's quality settings. Try the
   rewind fix's effect first, then idle-wait elision.
3. Presentation-thread temporal reversal (owner keeps `present_thread = false`).
4. FEEDBACK_BURNDOWN items: distant culling, widescreen camera comfort, HD
   rewind, resident loading.

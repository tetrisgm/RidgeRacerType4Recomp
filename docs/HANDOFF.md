# Handoff

## Current — 2026-11-08

Work on game `master` on the Mac only. Engine fork `master` now points to the
unchanged verified pin `006fb5bb`; UI remains upstream `817c2985`. Three fixes
are in [upstream PRs #860–#862](UPSTREAM.md). No regeneration or pin bump.
Mac Release, CTest 26/26, hidden/dummy-audio 40-step smoke (zero misses),
rewind ownership regression and real-GL HD fixture (19 checks) pass.

Windows x64 cross-compilation and linking succeeded with llvm-mingw on the
Mac; PE imports are Windows/UCRT only. See [build instructions and limits](WINDOWS_BUILD.md).
No Windows execution, STEAMBOX access or NAS delivery occurred.

## Blockers / owner review

- Complete native R4.BIN overlay cache coverage must replace the legacy
  runtime compiler before a finished compiler-free Windows delivery. Local
  candidates carry `DELIVERY_BLOCKED.txt`. No `builds` share was visible on NAS.
- [Branch dispositions and exact delete commands](archive/BRANCH_DISPOSITION_20261108.md)
  await approval. The public game EXE on `backup/.../pc-wip/r4-mstan-validation`
  remains untouched pending the owner's separate decision.
- Proposed workflow diff: [CLAUDE patch](archive/CLAUDE-proposed-20261108.patch).
  `CLAUDE.md` is unchanged until approved. The current owner's task supersedes
  its stale recovery/fork/PC instructions.
- 45 old game worktrees and old topic branches still exist; none was removed.
  Preserve their local data before retiring them. Existing untracked
  `recomp-ui/third_party/imgui` is untouched.

## Next

1. Resolve native overlay packaging, then deliver a fresh checksummed version
   when the owner-created NAS share is available. Owner validates race boot,
   the normal-paced rank-8 route and 4K/Max Detail slowdown on STEAMBOX.
2. Follow upstream PR review; once merged, qualify/record a new upstream pin,
   change the URL and archive the fork.
3. Apply approved contract/deletions. Research leads live in game issues
   [#108](https://github.com/tetrisgm/RidgeRacerType4Recomp/issues/108) and
   [#109](https://github.com/tetrisgm/RidgeRacerType4Recomp/issues/109).

Historical handoff: [archive](archive/HANDOFF-before-consolidation-20261108.md).
Current performance evidence: [race investigation](recovery/PERFORMANCE_HANDOFF_20261105.md).
Full PC copy: NAS `/volume3/pcbackup/pc-20261106/`.

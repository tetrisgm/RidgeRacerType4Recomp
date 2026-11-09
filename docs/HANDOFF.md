# Handoff

## Current — 2026-11-09

Mac-only development on game `master`; engine remains fork master `006fb5bb`,
UI upstream `817c2985`. [Upstream PRs #860–#862](UPSTREAM.md) remain the path
back to upstream. The owner-approved `CLAUDE.md` workflow is applied.

All **36 approved backup/archive/recovery branches are deleted**, including
the public-EXE branch and our 17 maintainer-repo backups. [NAS preservation
receipt](archive/BRANCH_PRESERVATION_20261109.json): all tips reconstructed and
full Git integrity verified; EXE preserved. Original PC archive is unchanged.

Native overlay packaging is implemented: nine distinct original-disc images,
7,459 guarded variants linked into the game, committed generated output and
identity checks. No runtime compiler/cache is needed. See [overlay evidence](../aot/README.md)
and [Windows build/delivery instructions](WINDOWS_BUILD.md).

## Verification and delivery

Mac Release/CTest and isolated hidden smoke pass. Native smoke records 7.19M
overlay dispatches, zero dispatch/segment misses and no interpreted R4-overlay
PCs on the tested route. Windows x64 links all variants; imports are system/UCRT
only, with checksummed versioned folders. Windows execution is not claimed.
No STEAMBOX connection or operation occurred.

The NAS `builds` share remains unavailable; retain the final package under
`dist/windows/` locally. Once the share exists, use `tools/deliver_windows.py`
with a fresh version. The owner tests race boot, the normal-paced rank-8 route,
and 4K/Max Detail slowdown, preserving `present_thread = false`.

## Next

- Follow upstream review, then qualify/record the upstream pin and archive the fork.
- Owner Windows playtest after NAS delivery; exhaustive mode/indirect-target
  coverage is not established by the smoke route.
- Old topic branches and 45 worktrees were outside the approved deletion list
  and remain. Preserve their local data before retirement. Existing untracked
  `recomp-ui/third_party/imgui` was left untouched.
- Research leads: game issues [#108](https://github.com/tetrisgm/RidgeRacerType4Recomp/issues/108)
  and [#109](https://github.com/tetrisgm/RidgeRacerType4Recomp/issues/109).

Full PC archive: NAS `/volume3/pcbackup/pc-20261106/`. Additional verified
preservation: `/volume2/homes/theadminaccount/r4-consolidation-20261109/`.

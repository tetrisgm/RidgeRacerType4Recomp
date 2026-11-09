# Branch disposition — 2026-11-08

All 36 listed branches were deleted on 2026-11-09 after owner approval and NAS preservation verification; see [receipt](BRANCH_PRESERVATION_20261109.json). Counts are ancestry counts, not a claim that every unique patch belongs in the current architecture. Game base: `87fc0234`; fork master after fast-forward: `006fb5bb`; upstream master: `9b914263`. The full PC copy remains at NAS `/volume3/pcbackup/pc-20261106/` (directory confirmed present).

| Repository | Branch | Unique commits vs master | Contents / reference | Verdict |
|---|---|---:|---|---|
| `tetrisgm/RidgeRacerType4Recomp` | `archive/original-20261102` | 0 | Ancestor of master | on master |
| `tetrisgm/RidgeRacerType4Recomp` | `backup/pc-20261106/launcher-xnet-20261008-r4/feat/launcher-surface` | 11 | Obsolete custom launcher surface and pin changes | superseded |
| `tetrisgm/RidgeRacerType4Recomp` | `backup/pc-20261106/launcher-xnet-20261008-r4/pc-wip/launcher-xnet-20261008__r4` | 12 | Obsolete custom launcher surface and pin changes | superseded |
| `tetrisgm/RidgeRacerType4Recomp` | `backup/pc-20261106/perf-lowend-20261008-r4/pc-wip/perf-lowend-20261008__r4` | 1 | Idle-wait/store-counter lead; [#108](https://github.com/tetrisgm/RidgeRacerType4Recomp/issues/108), `f64a12f90a38` | worth reviving |
| `tetrisgm/RidgeRacerType4Recomp` | `backup/pc-20261106/ridgeracertype4/fix/race-real-time` | 12 | Superseded frame-architecture plans/evidence and pin changes | superseded |
| `tetrisgm/RidgeRacerType4Recomp` | `backup/pc-20261106/ridgeracertype4/pc-wip-tracked/r4-wt-release-lap` | 8 | Performance/audit research only; [#109](https://github.com/tetrisgm/RidgeRacerType4Recomp/issues/109); `287a7795ddad` | worth reviving |
| `tetrisgm/RidgeRacerType4Recomp` | `backup/pc-20261106/ridgeracertype4/pc-wip/r4-mstan-validation` | 1 | Recovered Max Detail/performance notes; Public-EXE branch deleted after NAS preservation | on master |
| `tetrisgm/RidgeRacerType4Recomp` | `backup/pc-20261106/ridgeracertype4/pc-wip/r4-wt-final` | 1 | Old pin changes only | superseded |
| `tetrisgm/RidgeRacerType4Recomp` | `backup/pc-20261106/ridgeracertype4/pc-wip/r4-wt-interp` | 1 | Empty WIP snapshot | superseded |
| `tetrisgm/RidgeRacerType4Recomp` | `backup/pc-20261106/ridgeracertype4/pc-wip/r4-wt-pgxp-audit` | 8 | Performance/audit research only; [#109](https://github.com/tetrisgm/RidgeRacerType4Recomp/issues/109); `7ea239c0c104` | worth reviving |
| `tetrisgm/RidgeRacerType4Recomp` | `backup/pc-20261106/ridgeracertype4/pc-wip/ridgeracertype4` | 13 | Superseded frame-architecture plans/evidence and pin changes | superseded |
| `tetrisgm/RidgeRacerType4Recomp` | `recovery/upstream-20261102` | 0 | Ancestor of master | on master |
| `tetrisgm/psxrecomp` | `archive/r4-original-20261102` | 321 | Superseded fork architecture; 84d6ca22259a | superseded |
| `tetrisgm/psxrecomp` | `backup/pc-20261106/ridgeracertype4-wt-r4-mstan-validation-psxrecomp/pc-wip/r4-mstan-validation__psxrecomp` | 1 | XA change is patch-identical to master 006fb5bb (different commit identity) | on master |
| `tetrisgm/psxrecomp` | `backup/pc-20261106/ridgeracertype4-wt-r4-wt-interp-psxrecomp/pc-wip/r4-wt-interp__psxrecomp` | 293 | Superseded fork architecture; 6e58b8bfcfdb | superseded |
| `tetrisgm/psxrecomp` | `backup/pc-20261106/ridgeracertype4-wt-r4-wt-pgxp-audit-psxrecomp/fix/pooled-restage-validation` | 305 | Performance/audit research only; [#109](https://github.com/tetrisgm/RidgeRacerType4Recomp/issues/109); `935347567358` | worth reviving |
| `tetrisgm/psxrecomp` | `backup/pc-20261106/ridgeracertype4-wt-r4-wt-pgxp-audit-psxrecomp/pc-wip/psx-wt-restage-validation` | 306 | Performance/audit research only; [#109](https://github.com/tetrisgm/RidgeRacerType4Recomp/issues/109); `5d360862e0ba` | worth reviving |
| `tetrisgm/psxrecomp` | `backup/pc-20261106/ridgeracertype4-wt-r4-wt-pgxp-audit-psxrecomp/pc-wip/r4-wt-pgxp-audit__psxrecomp` | 304 | Performance/audit research only; [#109](https://github.com/tetrisgm/RidgeRacerType4Recomp/issues/109); `7e4aa8342808` | worth reviving |
| `tetrisgm/psxrecomp` | `recovery/upstream-20261102` | 0 | Ancestor of master | on master |
| `RetroPortingToolKit/psxrecomp` | `backup/pc-20261106/ridgeracertype4-psxrecomp/feat/frame-arch-1-native-authority` | 210 | Superseded fork architecture; 464bc104797b | superseded |
| `RetroPortingToolKit/psxrecomp` | `backup/pc-20261106/ridgeracertype4-psxrecomp/feat/frame-arch-2-mailbox` | 219 | Superseded fork architecture; 27d6d16796c7 | superseded |
| `RetroPortingToolKit/psxrecomp` | `backup/pc-20261106/ridgeracertype4-psxrecomp/feat/frame-arch-3-surfaces` | 245 | Superseded fork architecture; 499a12abd0af | superseded |
| `RetroPortingToolKit/psxrecomp` | `backup/pc-20261106/ridgeracertype4-psxrecomp/feat/menu-main-screen` | 197 | Superseded fork architecture; cc3a08b402a7 | superseded |
| `RetroPortingToolKit/psxrecomp` | `backup/pc-20261106/ridgeracertype4-psxrecomp/feat/render-pass-fps-internal-scale` | 1 | Old mod render-pass rate API; recovery uses original race cadence | superseded |
| `RetroPortingToolKit/psxrecomp` | `backup/pc-20261106/ridgeracertype4-psxrecomp/feat/rewind-title-default` | 196 | Superseded fork architecture; be636e415dc6 | superseded |
| `RetroPortingToolKit/psxrecomp` | `backup/pc-20261106/ridgeracertype4-psxrecomp/fix/async-expiry` | 160 | Superseded fork architecture; 35b3849e127c | superseded |
| `RetroPortingToolKit/psxrecomp` | `backup/pc-20261106/ridgeracertype4-psxrecomp/fix/display-playtest-20261028` | 202 | Superseded fork architecture; a7122c591ed2 | superseded |
| `RetroPortingToolKit/psxrecomp` | `backup/pc-20261106/ridgeracertype4-psxrecomp/fix/gl-sampler-uniforms` | 164 | Superseded fork architecture; 6eead0f2ad2d | superseded |
| `RetroPortingToolKit/psxrecomp` | `backup/pc-20261106/ridgeracertype4-psxrecomp/fix/launcher-playtest-20261027` | 193 | Superseded fork architecture; 90800120592c | superseded |
| `RetroPortingToolKit/psxrecomp` | `backup/pc-20261106/ridgeracertype4-psxrecomp/fix/prefer-steam-virtual-pad` | 201 | Superseded fork architecture; 487ec0422d23 | superseded |
| `RetroPortingToolKit/psxrecomp` | `backup/pc-20261106/ridgeracertype4-psxrecomp/fix/queued-vram-snapshot` | 205 | Performance/audit research only; [#109](https://github.com/tetrisgm/RidgeRacerType4Recomp/issues/109); `1efd81f9c460` | worth reviving |
| `RetroPortingToolKit/psxrecomp` | `backup/pc-20261106/ridgeracertype4-psxrecomp/fix/race-cadence-metrics` | 203 | Superseded fork architecture; ac65fcf5aa13 | superseded |
| `RetroPortingToolKit/psxrecomp` | `backup/pc-20261106/ridgeracertype4-psxrecomp/fix/steam-controller-outside-steam` | 197 | Superseded fork architecture; 80b058e6e133 | superseded |
| `RetroPortingToolKit/psxrecomp` | `backup/pc-20261106/ridgeracertype4-psxrecomp/int/threaded-rendering` | 133 | Performance/audit research only; [#109](https://github.com/tetrisgm/RidgeRacerType4Recomp/issues/109); `040cb5592b80` | worth reviving |
| `RetroPortingToolKit/psxrecomp` | `backup/pc-20261106/ridgeracertype4-psxrecomp/pc-wip/ridgeracertype4__psxrecomp` | 246 | Superseded fork architecture; 23698e8ec000 | superseded |
| `RetroPortingToolKit/psxrecomp` | `backup/pc-20261106/ridgeracertype4-psxrecomp/r4/main` | 225 | Superseded fork architecture; a8a62e54a73f | superseded |

“On master” for the game mstan-validation row refers only to useful source/docs fixes. The EXE and disc-side files deliberately remain excluded from master. Branch deletion does not guarantee removal of public GitHub object caches; no history rewrite is authorized.

## Approved deletion commands — completed 2026-11-09

These are the exact commands approved by the owner; deletion was batched atomically per repository after every tip was rechecked and its NAS history verified. No upstream maintainer branch other than our `backup/pc-20261106/*` refs is included.

```sh
git push https://github.com/tetrisgm/RidgeRacerType4Recomp.git --delete 'archive/original-20261102'
git push https://github.com/tetrisgm/RidgeRacerType4Recomp.git --delete 'backup/pc-20261106/launcher-xnet-20261008-r4/feat/launcher-surface'
git push https://github.com/tetrisgm/RidgeRacerType4Recomp.git --delete 'backup/pc-20261106/launcher-xnet-20261008-r4/pc-wip/launcher-xnet-20261008__r4'
git push https://github.com/tetrisgm/RidgeRacerType4Recomp.git --delete 'backup/pc-20261106/perf-lowend-20261008-r4/pc-wip/perf-lowend-20261008__r4'
git push https://github.com/tetrisgm/RidgeRacerType4Recomp.git --delete 'backup/pc-20261106/ridgeracertype4/fix/race-real-time'
git push https://github.com/tetrisgm/RidgeRacerType4Recomp.git --delete 'backup/pc-20261106/ridgeracertype4/pc-wip-tracked/r4-wt-release-lap'
git push https://github.com/tetrisgm/RidgeRacerType4Recomp.git --delete 'backup/pc-20261106/ridgeracertype4/pc-wip/r4-mstan-validation'
git push https://github.com/tetrisgm/RidgeRacerType4Recomp.git --delete 'backup/pc-20261106/ridgeracertype4/pc-wip/r4-wt-final'
git push https://github.com/tetrisgm/RidgeRacerType4Recomp.git --delete 'backup/pc-20261106/ridgeracertype4/pc-wip/r4-wt-interp'
git push https://github.com/tetrisgm/RidgeRacerType4Recomp.git --delete 'backup/pc-20261106/ridgeracertype4/pc-wip/r4-wt-pgxp-audit'
git push https://github.com/tetrisgm/RidgeRacerType4Recomp.git --delete 'backup/pc-20261106/ridgeracertype4/pc-wip/ridgeracertype4'
git push https://github.com/tetrisgm/RidgeRacerType4Recomp.git --delete 'recovery/upstream-20261102'
git push https://github.com/tetrisgm/psxrecomp.git --delete 'archive/r4-original-20261102'
git push https://github.com/tetrisgm/psxrecomp.git --delete 'backup/pc-20261106/ridgeracertype4-wt-r4-mstan-validation-psxrecomp/pc-wip/r4-mstan-validation__psxrecomp'
git push https://github.com/tetrisgm/psxrecomp.git --delete 'backup/pc-20261106/ridgeracertype4-wt-r4-wt-interp-psxrecomp/pc-wip/r4-wt-interp__psxrecomp'
git push https://github.com/tetrisgm/psxrecomp.git --delete 'backup/pc-20261106/ridgeracertype4-wt-r4-wt-pgxp-audit-psxrecomp/fix/pooled-restage-validation'
git push https://github.com/tetrisgm/psxrecomp.git --delete 'backup/pc-20261106/ridgeracertype4-wt-r4-wt-pgxp-audit-psxrecomp/pc-wip/psx-wt-restage-validation'
git push https://github.com/tetrisgm/psxrecomp.git --delete 'backup/pc-20261106/ridgeracertype4-wt-r4-wt-pgxp-audit-psxrecomp/pc-wip/r4-wt-pgxp-audit__psxrecomp'
git push https://github.com/tetrisgm/psxrecomp.git --delete 'recovery/upstream-20261102'
git push https://github.com/RetroPortingToolKit/psxrecomp.git --delete 'backup/pc-20261106/ridgeracertype4-psxrecomp/feat/frame-arch-1-native-authority'
git push https://github.com/RetroPortingToolKit/psxrecomp.git --delete 'backup/pc-20261106/ridgeracertype4-psxrecomp/feat/frame-arch-2-mailbox'
git push https://github.com/RetroPortingToolKit/psxrecomp.git --delete 'backup/pc-20261106/ridgeracertype4-psxrecomp/feat/frame-arch-3-surfaces'
git push https://github.com/RetroPortingToolKit/psxrecomp.git --delete 'backup/pc-20261106/ridgeracertype4-psxrecomp/feat/menu-main-screen'
git push https://github.com/RetroPortingToolKit/psxrecomp.git --delete 'backup/pc-20261106/ridgeracertype4-psxrecomp/feat/render-pass-fps-internal-scale'
git push https://github.com/RetroPortingToolKit/psxrecomp.git --delete 'backup/pc-20261106/ridgeracertype4-psxrecomp/feat/rewind-title-default'
git push https://github.com/RetroPortingToolKit/psxrecomp.git --delete 'backup/pc-20261106/ridgeracertype4-psxrecomp/fix/async-expiry'
git push https://github.com/RetroPortingToolKit/psxrecomp.git --delete 'backup/pc-20261106/ridgeracertype4-psxrecomp/fix/display-playtest-20261028'
git push https://github.com/RetroPortingToolKit/psxrecomp.git --delete 'backup/pc-20261106/ridgeracertype4-psxrecomp/fix/gl-sampler-uniforms'
git push https://github.com/RetroPortingToolKit/psxrecomp.git --delete 'backup/pc-20261106/ridgeracertype4-psxrecomp/fix/launcher-playtest-20261027'
git push https://github.com/RetroPortingToolKit/psxrecomp.git --delete 'backup/pc-20261106/ridgeracertype4-psxrecomp/fix/prefer-steam-virtual-pad'
git push https://github.com/RetroPortingToolKit/psxrecomp.git --delete 'backup/pc-20261106/ridgeracertype4-psxrecomp/fix/queued-vram-snapshot'
git push https://github.com/RetroPortingToolKit/psxrecomp.git --delete 'backup/pc-20261106/ridgeracertype4-psxrecomp/fix/race-cadence-metrics'
git push https://github.com/RetroPortingToolKit/psxrecomp.git --delete 'backup/pc-20261106/ridgeracertype4-psxrecomp/fix/steam-controller-outside-steam'
git push https://github.com/RetroPortingToolKit/psxrecomp.git --delete 'backup/pc-20261106/ridgeracertype4-psxrecomp/int/threaded-rendering'
git push https://github.com/RetroPortingToolKit/psxrecomp.git --delete 'backup/pc-20261106/ridgeracertype4-psxrecomp/pc-wip/ridgeracertype4__psxrecomp'
git push https://github.com/RetroPortingToolKit/psxrecomp.git --delete 'backup/pc-20261106/ridgeracertype4-psxrecomp/r4/main'
```

## Other branches and local worktrees

The three `codex/*` engine branches supporting PRs #860–#862 are temporary review branches. Retire them only after their PRs land. Old `r4/main` and topic branches are not current development lines. The full head inventory is in [remote-heads-20261108.tsv](remote-heads-20261108.tsv). No old topic branch is silently deleted.

45 game worktrees remain registered. Their removal is pending preservation checks; the new workflow creates none. The untracked `recomp-ui/third_party/imgui` nested clone existed before this task and was left alone.

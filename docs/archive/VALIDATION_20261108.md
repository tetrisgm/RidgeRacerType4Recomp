# Consolidation validation — 2026-11-08

Game source base `87fc0234`, engine `006fb5bb`, UI `817c2985`.
No generated sources or engine/UI pins changed. Mac-only execution.

- Release configure with `R4_BUILD_TESTS=ON`, `R4_PGXP=ON`,
  `PSX_DEBUG_TOOLS=ON`, enhanced profile; build succeeds.
- CTest: 26/26 pass (local verified USA disc present).
- `tools/smoke.py`: 40 steps against a copied, isolated runtime, hidden
  window and `SDL_AUDIODRIVER=dummy`, port 4798. Final race frame 8481;
  zero total/unique dispatch and segment misses. Only the process started
  by the harness was terminated. No player settings/saves were copied back.
- `test_rewind_load_context.c`: optimized actual-ring regression passes,
  including HD provenance on accepted/rejected restores, budget and eviction.
- `run_gl_texture_filter.py --hd-pack`: real hidden GL, 19 checks, zero
  failures; original/native and hires texture oracles report zero bad pixels.
- llvm-mingw 20261104 UCRT: Windows x64 Release links. Only Windows system
  and UCRT imports; no compiler/runtime DLL to bundle. OpenBIOS-only candidate
  produced by `tools/build_windows.sh`; 110 SHA256 manifest entries checked.
  A second attempt at the same version is rejected before building/copying.
- Shell syntax, Python compilation, current-document local references and
  `git diff --check` pass. Recursive submodules match their gitlinks.

PRs #860–#862 contain separate fixes. Validation is of the integrated R4 pin,
not an assertion that current upstream master or Windows gameplay was tested.
PR #861's conflicting recovery-only timing-log addition was removed in an
additive commit; its renderer/test patch is unchanged. No force-push.

Local raw evidence: `analysis/consolidation-20261108/` (isolated runtime,
smoke images/logs, HD fixture receipts, rewind test). Build command outputs
are `/tmp/r4-*-*.log` for this session and are not durable qualification data.
See [Windows blocker](../WINDOWS_BUILD.md) before promoting any local candidate.

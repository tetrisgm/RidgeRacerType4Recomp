# Native packaging and approved cleanup — 2026-11-09

Engine `006fb5bb`, UI `817c2985`; neither pin changed. Boot-EXE generated C
is unchanged. The new static overlay output comes from the supported framework
AOT pipeline, never manual generated-code edits.

- Approved `CLAUDE.md` diff applied in `40becd00`.
- All 36 listed branches preserved on NAS and deleted atomically per repo.
  [Preservation receipt](BRANCH_PRESERVATION_20261109.json): remote tips matched
  the approved inventory; full NAS-reconstructed Git histories passed fsck;
  public-EXE commit/blob preserved. Remote absence verified after deletion.
- Nine unique images cover the 13 R4.BIN code slots (slot 664 is text).
  Disc SHA-1, table words, file bounds and per-image SHA-256 verified by the
  [profile](../../aot/overlays.json). Static audit: 7,459 guarded variants in ten
  C units, all guards match original bytes. CPS agrees with the game; PGXP
  hooks are compiled with the runtime's enabled PGXP definitions.
- Mac Release build passes. CTest 28/28 includes static-output identity and
  delivery checks. Delivery tests cover successful verified copy, reused
  version rejection without overwrite, absent share preservation, payload
  tampering, unlisted nested files and dirty-build rejection.
- Final isolated `tools/verify_native_overlays.py` run: 7,210,566 static hits,
  zero dispatch/segment misses, zero interpreted PCs in the R4 overlay range.
  Forty-step hidden/dummy-audio smoke reaches the race. No external native
  cache, compiler, SDK or private profile is present in the isolated folder.
  A race screenshot was inspected; this is not normal-paced crash-route or
  exhaustive game-mode qualification.
- Windows x64 Release links all 7,459 variant symbols and the static dispatcher.
  Package checks enforce generated audit hashes, AMD64 PE identity,
  system/UCRT-only imports and SHA256 for every delivered file.
- Source/script whitespace and reference checks pass. The upstream emitter
  produced 14 whitespace-only lines in one generated unit; generated output
  and its audited hash are preserved without manual edits.
- No Windows code executed. No STEAMBOX connection or operation occurred.
  No builds share was visible on NAS; output remains local. Archive additions
  only used the existing NAS home, with no share/permission changes.

The generator's `full_static_coverage_proven: false` remains truthful: these
checks cannot prove every indirect target or self-modified-code state across
all modes. Windows gameplay and the owner's rank-8/4K slowdown routes remain
manual owner tests. No interpreter fallback was introduced to solve packaging.

Raw local evidence: `analysis/consolidation-20261109/`, including original-disc
AOT recipes (local game bytes; never delivered), native smoke receipts, and
NAS verification/deletion receipts. Package entry point and checks are in
[WINDOWS_BUILD.md](../WINDOWS_BUILD.md).

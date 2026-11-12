# Windows builds from the Mac

## Verified route

macOS arm64 → Windows x86-64 works with **llvm-mingw 20261104 UCRT**.
No MSVC, Windows VM or GitHub Actions is needed. The executable links all nine
distinct USA R4.BIN code-overlay images (13 archive slots) directly into native
code: 7,459 original-byte-guarded variants. No runtime compiler, Python, SDK,
overlay DLL cache or captured game bytes are packaged.

The [overlay profile and regeneration instructions](../aot/README.md) describe
the disc extraction, duplicate slots and audit. Mac hidden smoke exercises the
linked code with no R4-overlay interpreter PCs, dispatch misses or segment
misses on its boot-to-race route. PGXP remains enabled. This is not exhaustive
mode/indirect-target coverage; the generator explicitly retains
`full_static_coverage_proven: false`. Native Windows playtest results remain owner-only. Optional Windows PE
execution through CrossOver on the Mac is recorded as compatibility evidence.

PE checks verify AMD64, all 7,459 linked variant symbols and system/UCRT-only
imports. Source files must match the original-disc audit before staging.
Each package records that evidence in `NATIVE-OVERLAYS.json` and `BUILD.json`.

Builds stay on the Mac under `dist/windows/<version>/`. The NAS has no role
in development, packaging or delivery. The owner chooses how to transfer the
portable folder to the gaming PC.

## Toolchain setup (Mac only)

Install Xcode command-line tools, Homebrew CMake, Ninja and Python 3.11+.
Download the pinned [official llvm-mingw release](https://github.com/mstorsjo/llvm-mingw/releases/tag/20261104):

```sh
mkdir -p "$HOME/.local/toolchains"
cd "$HOME/.local/toolchains"
curl -fLO https://github.com/mstorsjo/llvm-mingw/releases/download/20261104/llvm-mingw-20261104-ucrt-macos-universal.tar.xz
printf '%s  %s\n' 66cbd5178c4a71b0d473e68ede172f8eb7d15d023e67f39201d0a69e21f6bf3c llvm-mingw-20261104-ucrt-macos-universal.tar.xz | shasum -a 256 -c -
tar -xJf llvm-mingw-20261104-ucrt-macos-universal.tar.xz
```

The download hash was checked against the official release asset digest.
`LLVM_MINGW_ROOT` may override the extracted toolchain directory. Nothing is
installed on Windows. The CMake toolchain isolates target library/header
searches from Homebrew. It preincludes `<exception>` for pinned toml11, which
uses `std::terminate` without declaring it under libc++.

## One build command

From a recursively initialized `master` checkout:

```sh
JOBS=2 bash tools/build_windows.sh 2026.11.11-release-01
```

Use a **new version every time**. Output is `dist/windows/<version>/` with
`BUILD.json`, PE imports, runtime assets, OpenBIOS, license notices and
`SHA256SUMS`. Release builds enable runtime IPO/LTO and tracing stays off.
Fresh package defaults select 2160-line rendering, the 60-FPS GP enhancement
and hold-to-rewind. The Windows presentation worker remains off. Only bundled
mods and new defaults are staged; build-directory player state is excluded.
`DELIVERY.json` records signing and execution qualification separately.
The script refuses an existing version, stale overlay output,
missing linked variants or unexpected DLL imports, and rechecks every manifest
entry. The build/staging command does not execute the Windows binary. Earlier `2026-11-08-local-*` candidates
remain blocked and must not be delivered.

No game disc, boot EXE, retail BIOS, player saves/settings, SDK or overlay
compiler is copied. The executable accepts the owner's disc by path:

```text
r4-runtime.exe --disc "D:\your-disc\R4.cue"
```

OpenBIOS is bundled; the candidate requires no retail BIOS. Windows 11 already
provides UCRT. There is no installer, administrator requirement or system
change. A writable local folder holds the game's own state. Do not reuse the
legacy `tools/package_release.sh` for STEAMBOX: it bundles an overlay compiler.

## Optional verified copy

A finished package is already usable as a portable folder. To copy a clean,
checksummed build to an existing local or removable destination:

```sh
python3 tools/deliver_windows.py dist/windows/2026-11-10-4k-performance-01 --destination-root /Volumes/Transfer
```

The helper reserves `r4/<version>/` exclusively and verifies the copied files.
It refuses dirty builds, checksum discrepancies and reused versions. No NAS,
network share, remote connection or PC tooling is required.

## Optional Mac-hosted PE compatibility check

Use an existing isolated CrossOver bottle, never a player's bottle. The runtime
and test files stay under the new analysis directory. The helper forces the
standard `Z:` root mapping so debug image paths and player files stay isolated.

```sh
python3 tools/verify_native_overlays.py analysis/windows-release-check \
  --build dist/windows/VERSION --crossover-bottle R4ReleaseVerification20261111 \
  --menu-script tests/custom_menu.txt
```

This checks the actual PE's startup, native overlays, menu, controls and saved
settings using CrossOver on macOS. It does not establish native Windows GPU
performance or physical Steam Controller behavior. The receipt names the host
and executable hash; Windows signing status is explicit in `DELIVERY.json`.

## Owner test checklist

After a qualified folder is delivered:

1. Select the owner's USA disc path and boot through menus into an active race.
2. Replay the rank-8 crash route at normal pacing, with Max Detail and expanded
   8 MB RAM; report any hang and the build version.
3. Check the in-game menu and physical Steam Controller through direct launch
   and Steam Input. Verify buttons, sticks, menu open/close and no duplicate
   keyboard actions.
4. Check crowded-race audio and game speed at 4K/Max Detail. Preserve
   `present_thread = false` and the owner's native saves/settings.

These are owner tests, not results from the Mac checks.

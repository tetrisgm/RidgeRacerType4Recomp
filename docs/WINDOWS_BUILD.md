# Windows builds from the Mac

## Verified route and remaining blocker

macOS arm64 → Windows x86-64 works with **llvm-mingw 20261104 UCRT**.
The pinned engine/UI and generated R4 C compiled and linked successfully on
2026-11-08. No MSVC, Windows VM or GitHub Actions is needed. PE inspection
found only Windows system and UCRT imports, no VC++ redistributable or compiler
DLL dependency. Windows execution is **not** verified; only the owner operates
STEAMBOX.

The local candidate is not yet a completed compiler-free game package:
R4's dynamically loaded `R4.BIN` code overlays need a complete, validated native
cache. The old packager supplies a runtime compiler; that is prohibited on
STEAMBOX. The new script intentionally includes no compiler or captured disc
bytes. Without prepared native shards the existing runtime falls back for
uncached overlay regions. This is a delivery blocker, not a proposed solution.
Prepare original-disc overlay inventories and compile all required shards on
the Mac using the framework's `aot_overlay_pipeline.py` / `compile_overlays.py`
(`--target-os win --arch-abi win-x64`), then prove cache identity/coverage and
native loading on the Mac counterpart before removing the delivery block.
Do not ship the capture JSON: it contains original game bytes.

On 2026-11-08, read-only `ssh nas` inspection confirmed the PC archive at
`/volume3/pcbackup/pc-20261106/` and found no visible `/volume*/builds` directory.
No share or permission was created. Output therefore stays local.

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
JOBS=2 bash tools/build_windows.sh 2026-11-08-local-01
```

Use a **new version every time**. Output is `dist/windows/<version>/` with
`BUILD.json`, PE imports, runtime assets, OpenBIOS, license notices and
`SHA256SUMS`. The script refuses an existing version or unexpected DLL imports
and rechecks every manifest entry. `DELIVERY_BLOCKED.txt` prevents confusing
this candidate with a qualified game package. No Windows binary is executed.

No game disc, boot EXE, retail BIOS, player saves/settings, SDK or overlay
compiler is copied. The executable accepts the owner's disc by path:

```text
r4-runtime.exe --disc "D:\your-disc\R4.cue" --no-launcher
```

OpenBIOS is bundled; the candidate requires no retail BIOS. Windows 11 already
provides UCRT. There is no installer, administrator requirement or system
change. A writable local folder holds the game's own state. Do not reuse the
legacy `tools/package_release.sh` for STEAMBOX: it bundles an overlay compiler.

## NAS delivery after qualification

First resolve native overlay coverage and verify a new final candidate.
Before copying, confirm the owner-created `builds` share exists and is mounted
writable on the Mac; never create the share or change NAS permissions.
Create exactly one new `builds/r4/<version>/` directory with exclusive creation
(`mkdir` without `-p` for the version). Copy the candidate's contents, including
`SHA256SUMS`, and run `shasum -a 256 -c SHA256SUMS` in the destination.
If creation fails or the folder exists, stop: never overwrite or reuse it.
A failed/incomplete upload keeps that version reserved; retry under a new one.
STEAMBOX reads via NAS user `pc` on B:; the owner copies the folder locally.
Do not connect to or operate STEAMBOX from the Mac.

## Owner test checklist

After a qualified folder is delivered:

1. Select the owner's USA disc path and boot through menus into an active race.
2. Replay the rank-8 crash route at normal pacing, with Max Detail and expanded
   8 MB RAM; report any hang and the build version.
3. Check crowded-race audio and game speed at 4K/Max Detail. Preserve
   `present_thread = false` and the owner's native saves/settings.

These are owner tests, not results from the Mac checks.

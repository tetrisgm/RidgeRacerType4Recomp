#!/usr/bin/env bash
# Build this platform's bundled release zip(s): the compiled game, built from
# the committed generated/ C, with no disc data, no BIOS dump, no R4 or
# framework sources, no generated C and no emitters at the root
# (overlay_toolchain/ carries the emitters, runtime headers and a Python).
# Players unzip, run r4-runtime and pick their disc.
#
#   tools/package_release.sh [git-ref]        (default: HEAD)
#
#   macOS           -> dist/r4-<ver>-macos-arm64.zip + macos-x64.zip
#                      (one universal arm64+x86_64 build, ad-hoc signed; each
#                      zip carries its own overlay_toolchain/ Python)
#   Windows (MSYS2 MINGW64 shell) -> dist/r4-<ver>-windows-x64.zip
#
# Local only: no CI. Always packages from a throwaway clone at <ref>, so a
# dirty or untracked generated/ never reaches a zip, and the framework
# packager (which rewrites VERSION and stages into dist/) never runs in a
# working checkout. The psxrecomp and recomp-ui submodules are cloned from
# this checkout's own submodule repos, so a pin on a commit that is not
# upstream yet still resolves; their nested submodules come from their
# .gitmodules URLs. Text files ship byte-for-byte (no CRLF conversion).
#
# BIOS backends: OpenBIOS only by default, so the executable carries no code
# derived from a retail BIOS (Project Studio's audit forbids shipping it). A
# player who picks their own SCPH-1001 dump gets its backend built once on
# their machine by overlay_toolchain/ (psxrecomp docs/BIOS_SELECTION.md).
# R4_RELEASE_BIOS_STEMS="OpenBIOS;SCPH1001" links the framework's committed
# SCPH-1001 backend too, as psxrecomp's CI template does.
#
# Env: R4_RELEASE_DIR (clone location, default <repo>/build-release-clone),
#      R4_RELEASE_BIOS_STEMS (default OpenBIOS), JOBS (default: all cores),
#      R4_PYTHON (optional Python 3.11+ override for release checks).
set -euo pipefail

SRC="$(cd "$(dirname "$0")/.." && pwd)"
PYTHON="$(bash "$SRC/tools/select_python.sh")"
# The framework's overlay-toolchain stager parses TOML using its own Python
# process.  Give it the same interpreter selected for R4's release checks.
export PSX_RELEASE_STAGE_PYTHON="$PYTHON"
REF="${1:-HEAD}"
SHA="$(git -C "$SRC" rev-parse --verify "$REF^{commit}")"
REL="${R4_RELEASE_DIR:-$SRC/build-release-clone}"
BIOS_STEMS="${R4_RELEASE_BIOS_STEMS:-OpenBIOS}"
JOBS="${JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}"

case "$(uname -s)" in
  Darwin) PLATFORM=macos ;;
  MINGW*|MSYS*) PLATFORM=windows ;;
  *) echo "error: run on macOS or in an MSYS2 MINGW64 shell" >&2; exit 2 ;;
esac

# --- throwaway clone at <ref> -------------------------------------------------
# Absolute and space-free: REL feeds -ffile-prefix-map, which CMake splits on
# whitespace and which only matches the absolute paths the compiler sees.
mkdir -p "$REL"; REL="$(cd "$REL" && pwd -P)"
[[ "$REL" != *' '* ]] || { echo "error: release dir must not contain spaces: $REL" >&2; exit 2; }
rm -rf "$REL"
GITC=(-c core.autocrlf=false -c core.eol=lf)
git "${GITC[@]}" clone -q -c core.autocrlf=false "$SRC" "$REL"
git -C "$REL" checkout -q --detach "$SHA"
for sm in psxrecomp recomp-ui; do
  git -C "$REL" config "submodule.$sm.url" "$SRC/$sm"
done
git -C "$REL" "${GITC[@]}" -c protocol.file.allow=always submodule update -q --init
git -C "$REL/psxrecomp" "${GITC[@]}" submodule update -q --init --recursive
cd "$REL"

V="$(tr -d '[:space:]' < VERSION)"; V="${V#v}"
export RELEASE_VERSION="$V"
export EXCLUDE_DEV_MODS=1
echo "== r4 $V ($PLATFORM) from $SHA, BIOS backends: $BIOS_STEMS"
# Every game.toml [video] key is read by the pinned psxrecomp: a key set
# ahead of the pin would be inert in the zip, and R4's on-by-default display
# settings would not match its docs.
python3 tools/check_pin_keys.py
bash psxrecomp/tools/ci/record_pins.sh
# The committed game C is present and tracked, and the framework's committed
# BIOS backends match its emitter (the same gates psxrecomp's CI template runs).
bash psxrecomp/tools/ci/check_boot_exe.sh .
bash psxrecomp/tools/ci/check_generated.sh --root .
# Only the selected backends are linked.  In particular, an OpenBIOS-only
# package must not depend on the unused retail backend's emitter stamp.
IFS=';' read -r -a BIOS_STEM_ARRAY <<< "$BIOS_STEMS"
bash psxrecomp/tools/ci/check_bios_stamps.sh --framework psxrecomp "${BIOS_STEM_ARRAY[@]}"
# Later feature branches add this gate; use the same selected Python because
# check_pin_keys.py reads TOML with tomllib.
if [[ -f tools/check_pin_keys.py ]]; then
  "$PYTHON" tools/check_pin_keys.py
fi

EMIT=build-recompiler
HOST=build-release
EMIT_ARGS=(-DCMAKE_BUILD_TYPE=Release)
HOST_ARGS=(
  -DCMAKE_BUILD_TYPE=Release
  -DPSXRECOMP_REQUIRE_GAME_C=ON
  -DPSXRECOMP_BIOS_STALE_FATAL=ON
  -DPSXRECOMP_FORCE_SETUP_HOST=OFF
  -DPSXRECOMP_ALLOW_NO_BIOS=OFF
  "-DPSXRECOMP_BIOS_STEMS=$BIOS_STEMS"
  -DPSX_SETUP_WIZARD=ON
  -DPSX_GAME_VERSION="$V"
  -DPSX_ENABLE_VULKAN=OFF
  -DPSX_DEBUG_TOOLS=OFF
)
if [[ $PLATFORM == macos ]]; then
  # Universal, macOS 11+, no Homebrew dylibs (CMakeLists.txt sets the SDL3 and
  # FreeType defaults; repeated here so a stale cache cannot undo them), and no
  # build-machine paths embedded in the binaries.
  MAC=(-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
       "-DCMAKE_IGNORE_PREFIX_PATH=/opt/homebrew;/usr/local")
  EMIT_ARGS+=("${MAC[@]}")
  HOST_ARGS+=("${MAC[@]}"
              -DCMAKE_DISABLE_FIND_PACKAGE_SDL3=ON
              -DCMAKE_DISABLE_FIND_PACKAGE_Freetype=ON
              -DSDL_HIDAPI_LIBUSB=OFF)
  EXE=r4-runtime; SFX=""
  # The generated C is tens of MB; ccache makes a repackage of the same tree
  # close to a link (the clone path is fixed, so the prefix map matches).
  if command -v ccache >/dev/null 2>&1; then
    HOST_ARGS+=(-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache)
  fi
else
  # Static emitters (they ship in overlay_toolchain/), so a player's PATH can
  # never hand them the wrong runtime DLLs.
  EMIT_ARGS+=(-DPSXRECOMP_STATIC_CLI=ON)
  unset TOOLCHAIN_DIR PSXRECOMP_TOOLCHAIN_DIR RETCOMM_TOOLCHAIN_DIR BPE_TOOLCHAIN_DIR || true
  # The framework packager copies libgcc_s_seh-1, libstdc++-6 and
  # libwinpthread-1 from this dir into overlay_toolchain/ whether or not
  # anything imports them. Nothing does: r4-runtime.exe and both emitters are
  # static (gated below), and tcc and the embedded Python need none of them.
  # Point it at an empty dir so no unused GCC runtime ships; the packager's DLL
  # bundler still fails the release if the exe ever imports one.
  mkdir -p "$REL/.no-mingw-dlls"
  export PSXRECOMP_RUNTIME_BIN_DIR="$REL/.no-mingw-dlls"
  EXE=r4-runtime.exe; SFX=.exe
fi
# No build-machine paths in shipped binaries (__FILE__ in libjuice's logging,
# asserts, ...). The compiler sees native absolute paths: C:/... on Windows.
if [[ $PLATFORM == windows ]]; then NATIVE_REL="$(cygpath -m "$REL")"; else NATIVE_REL="$REL"; fi
MAP="-ffile-prefix-map=$NATIVE_REL/=./"
EMIT_ARGS+=("-DCMAKE_C_FLAGS=$MAP" "-DCMAKE_CXX_FLAGS=$MAP")
HOST_ARGS+=("-DCMAKE_C_FLAGS=$MAP" "-DCMAKE_CXX_FLAGS=$MAP")

cmake -S psxrecomp/recompiler -B "$EMIT" -G Ninja "${EMIT_ARGS[@]}" >/dev/null
cmake --build "$EMIT" --target psxrecomp-game psxrecomp-bios -j"$JOBS" >/dev/null
cmake -S . -B "$HOST" -G Ninja "${HOST_ARGS[@]}" > "$HOST.configure.log" 2>&1 \
  || { tail -40 "$HOST.configure.log" >&2; exit 1; }
# The configure took the full-game path and linked exactly the wanted BIOS.
grep -q 'linking generated game C (full runtime)' "$HOST.configure.log" \
  || { echo "error: configure did not link the generated game C" >&2; exit 1; }
grep -qxF -- "-- BIOS backends linked: $BIOS_STEMS" "$HOST.configure.log" \
  || { echo "error: BIOS backends linked are not '$BIOS_STEMS':" >&2; grep 'BIOS backends' "$HOST.configure.log" >&2; exit 1; }
cmake --build "$HOST" --target psx-runtime -j"$JOBS" >/dev/null

# --- gates: portable binaries, right version ----------------------------------
test "$(tr -d '[:space:]' < "$HOST/psx_game_version.txt")" = "$V"
BINS=("$HOST/$EXE" "$EMIT/psxrecomp-game$SFX" "$EMIT/psxrecomp-bios$SFX")
for b in "${BINS[@]}"; do
  if [[ $PLATFORM == macos ]]; then
    [[ "$(lipo -archs "$b")" == "x86_64 arm64" ]] || { echo "not universal: $b" >&2; exit 1; }
    if otool -L "$b" | grep -E '^[[:space:]]+[/@]' | grep -vE '^[[:space:]]+(/usr/lib/|/System/Library/)'; then
      echo "non-system dylib in $b" >&2; exit 1
    fi
    [[ "$(otool -l "$b" | awk '/LC_BUILD_VERSION/{f=1} f&&/minos/{print $2; f=0}' | sort -u)" == "11.0" ]] \
      || { echo "minos is not 11.0: $b" >&2; exit 1; }
  else
    # Static: no MinGW/SDL/zlib DLL imports, so none has to ship beside it.
    IMPORTS="$(objdump -p "$b" | awk '/DLL Name:/{print $3}')"
    [[ -n "$IMPORTS" ]] || { echo "no import table read from $b" >&2; exit 1; }
    if grep -iE '^(lib|sdl|zlib)' <<< "$IMPORTS"; then
      echo "non-system DLL imports above in $b" >&2; exit 1
    fi
  fi
  # Leak gate: the release dir and the builder's home, in every spelling the
  # binary could carry. strings goes to a file first -- grep -q on a pipe under
  # pipefail would SIGPIPE strings and hide a match.
  strings -a "$b" > .strings.txt
  LEAKS=("$REL" "$NATIVE_REL" "$HOME")
  if [[ $PLATFORM == windows ]]; then
    LEAKS+=("$(cygpath -m "$HOME")" "$(cygpath -w "$HOME")" "$(cygpath -w "$REL")")
    if [[ -n "${USERPROFILE:-}" ]]; then
      LEAKS+=("$USERPROFILE" "$(cygpath -m "$USERPROFILE")" "$(cygpath -u "$USERPROFILE")")
    fi
  fi
  for leak in "${LEAKS[@]}"; do
    if grep -qiF -- "$leak" .strings.txt; then
      echo "build-machine path leaked into $b: $leak" >&2; grep -iF -- "$leak" .strings.txt | head -3 >&2; exit 1
    fi
  done
done
rm -f .strings.txt

# --- sign (macOS: ad-hoc; no Developer ID, which would embed a personal name) -
# Before packaging, so the staged copies (the exe, and the emitters inside
# overlay_toolchain/) carry the signature. Windows signing is the framework
# packager's (tools/ci/sign_windows.sh), which skips when no certificate is set.
if [[ $PLATFORM == macos ]]; then
  for b in "${BINS[@]}"; do codesign --force --sign - "$b"; done
fi

# --- package -------------------------------------------------------------------
if [[ $PLATFORM == macos ]]; then ARTS=(macos-arm64 macos-x64); else ARTS=(windows-x64); fi
for A in "${ARTS[@]}"; do
  scripts/package_release.sh "$HOST" "$A" "$EMIT" > "dist-$A.log" 2>&1 \
    || { tail -40 "dist-$A.log" >&2; exit 1; }
done

# --- verify each zip (psxrecomp's CI template checks, plus R4's) -----------------
for A in "${ARTS[@]}"; do
  Z="dist/r4-$V-$A.zip"
  L="$("$PYTHON" -c 'import sys,zipfile; print("\n".join(zipfile.ZipFile(sys.argv[1]).namelist()))' "$Z")"
  # 1. It is the game: executable, OpenBIOS, runtime data, both R4 mods, and
  #    the notices its components' licenses ask for (recomp-ui's and TinyCC's
  #    come from the framework packager; Dear ImGui's, recomp-net's, retcomm-rbengine's and, on Windows, the MinGW-w64
  #    runtime's and winpthreads' come from scripts/package_release.sh).
  NEED=("$EXE" psx_game_version.txt bios/openbios.bin bios/OpenBIOS.LICENSE game.toml
        game_options.toml DISC.md LICENSE README.txt
        THIRD-PARTY-LICENSES/README.md licenses/psxrecomp-LICENSE
        licenses/recomp-ui-LICENSE assets/fonts/NOTICE.md assets/img/NOTICE.md
        licenses/dear-imgui-LICENSE.txt licenses/recomp-net-LICENSE
        licenses/retcomm-rbengine-LICENSE
        "overlay_toolchain/psxrecomp-game$SFX" "overlay_toolchain/psxrecomp-bios$SFX")
  if [[ $PLATFORM == windows ]]; then
    NEED+=(licenses/winpthreads-COPYING licenses/mingw-w64-runtime-COPYING.txt)
  fi
  for f in "${NEED[@]}"; do
    grep -qxF -- "$f" <<< "$L" || { echo "missing $f in $Z" >&2; exit 1; }
  done
  # No GCC runtime DLLs: nothing shipped imports them (see PSXRECOMP_RUNTIME_BIN_DIR).
  if grep -iE '(^|/)(libgcc_s_[^/]*|libstdc\+\+-[^/]*|libwinpthread-[^/]*)\.dll$' <<< "$L"; then
    echo "unused GCC runtime DLLs above in $Z" >&2; exit 1
  fi
  for p in assets/fonts/ licenses/ mods/bundled/r4.enhancement.widescreen/; do
    grep -q "^$p" <<< "$L" || { echo "missing $p in $Z" >&2; exit 1; }
  done
  # TinyCC (LGPL-2.1, Windows overlay compiler) must ship with its license
  # (psxrecomp THIRD_PARTY_ATTRIBUTION.md; the framework packager stages it). Windows players have no compiler of their own, so tcc must ship too.
  if [[ $PLATFORM == windows ]]; then
    for f in overlay_toolchain/tcc/tcc.exe overlay_toolchain/tcc/COPYING; do
      grep -qxF -- "$f" <<< "$L" || { echo "missing $f in $Z" >&2; exit 1; }
    done
  fi
  if grep -q '^overlay_toolchain/tcc/' <<< "$L" &&
     ! grep -iqE '^overlay_toolchain/tcc/([^/]+/)*[^/]*(copying|licen[cs]e|lgpl)[^/]*$' <<< "$L"; then
    echo "overlay_toolchain/tcc/ ships without the TinyCC LGPL-2.1 license in $Z" >&2; exit 1
  fi
  # 2. It is not a build kit: no framework tree, CLI, root emitters, sources,
  #    generated C, tools or tests. Source-like files are allowed only where the
  #    toolchain needs them: Python's and TinyCC's own trees, and the runtime
  #    headers (.h, .c.inc) in overlay_toolchain/include/.
  if grep -E '^(psxrecomp|recomp-ui|generated|seeds|annotations|src|tools|tests|toolchain|ghidra|disc|saves|cache|build[^/]*)/|^(psxrecomp_cli\.py|psxrecomp-game|psxrecomp-bios|CMakeLists\.txt|codegen_setup\.)' <<< "$L"; then
    echo "build-kit content above in $Z" >&2; exit 1
  fi
  if grep -iE '\.(c|cc|cpp|h|hpp|inc)$' <<< "$L" \
       | grep -vE '^overlay_toolchain/(python|tcc)/|^overlay_toolchain/include/[^/]+\.(h|inc)$'; then
    echo "sources above outside the toolchain's own trees in $Z" >&2; exit 1
  fi
  # 3. No disc, extracted EXE, BIOS dump or BIOS C, memory card, capture or
  #    cheat file; the only image is OpenBIOS. The BIOS profile TOMLs and seed
  #    lists the toolchain needs are the only files allowed in
  #    overlay_toolchain/{bios,recompiler}/, and the only SCPH-named ones.
  if grep -iE '(^|/)SCPH[^/]*$|SLUS_007\.97|\.(cue|iso|img|chd|ccd|sub|mcd|mcr|cht|rom)$|(^|/)(overlay_captures\.json|settings\.toml|bios\.cfg|disc\.cfg|state\.toml)$|^mods/installed/' <<< "$L" \
       | grep -vE '^overlay_toolchain/bios/[A-Za-z0-9_-]+\.toml$'; then
    echo "disc / BIOS / per-machine files above in $Z" >&2; exit 1
  fi
  if grep -E '^overlay_toolchain/(bios|recompiler)/' <<< "$L" \
       | grep -vE '^overlay_toolchain/bios/[A-Za-z0-9_-]+\.toml$|^overlay_toolchain/recompiler/seeds/[A-Za-z0-9_-]+\.json$'; then
    echo "files above in overlay_toolchain/{bios,recompiler}/ are not BIOS profiles or seed lists in $Z" >&2; exit 1
  fi
  if grep -iE '\.bin$' <<< "$L" | grep -vxF bios/openbios.bin; then
    echo "unexpected .bin above in $Z" >&2; exit 1
  fi
  # 4. The only bundled artwork is the HD HUD pack (Kuid0us/T4HDHUD, re-keyed
  #    by tools/r4_hd_hud_pack.py): images under mods/ live in its pack folder.
  if grep -iE '^mods/.*\.(png|jpe?g|bmp|tga|dds|webp)$' <<< "$L" \
       | grep -vE '^mods/bundled/r4\.enhancement\.ui-fonts/[^/]+/pack/[0-9a-f]{7,8}-[0-9a-f]{7,8}\.png$'; then
    echo "unexpected images above in $Z (only the HD HUD pack may ship)" >&2; exit 1
  fi
  # 5. No developer-channel mods.
  if "$PYTHON" - "$Z" <<'PY'
import re, sys, zipfile
z = zipfile.ZipFile(sys.argv[1])
dev = [n for n in z.namelist() if n.endswith("manifest.toml")
       and re.search(rb'^\s*channel\s*=\s*"developer"', z.read(n), re.M)]
print("\n".join(dev))
sys.exit(0 if dev else 1)
PY
  then echo "developer-channel mods above in $Z" >&2; exit 1; fi
  (cd dist && shasum -a 256 "r4-$V-$A.zip" 2>/dev/null || sha256sum "r4-$V-$A.zip") > "dist/r4-$V-$A.zip.sha256"
  echo "OK $Z ($(du -h "$Z" | cut -f1))"
done
echo "zips in $REL/dist"

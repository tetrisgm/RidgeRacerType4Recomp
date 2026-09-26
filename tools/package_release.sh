#!/usr/bin/env bash
# Build this platform's setup-host release zip(s): no game code, BIOS or disc.
# Players unzip, run r4-runtime, pick their own disc, and the wizard generates
# and compiles the game locally.
#
#   tools/package_release.sh [git-ref]        (default: HEAD)
#
#   macOS           -> dist/r4-<ver>-macos-arm64.zip + macos-x64.zip
#                      (one universal arm64+x86_64 build, ad-hoc signed)
#   Windows (MSYS2 MINGW64 shell) -> dist/r4-<ver>-windows-x64.zip
#
# Always packages from a throwaway clone at <ref>: the framework packager wipes
# generated/, rewrites VERSION and stages untracked framework files, so it must
# never run in a working checkout. Submodules are cloned from this checkout's
# own submodule repos, so unmerged framework commits in the pin still resolve.
#
# Env: R4_RELEASE_DIR (clone location, default <repo>/build-release-clone),
#      JOBS (default: all cores).
set -euo pipefail

SRC="$(cd "$(dirname "$0")/.." && pwd)"
REF="${1:-HEAD}"
SHA="$(git -C "$SRC" rev-parse --verify "$REF^{commit}")"
REL="${R4_RELEASE_DIR:-$SRC/build-release-clone}"
JOBS="${JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}"

case "$(uname -s)" in
  Darwin) PLATFORM=macos ;;
  MINGW*|MSYS*) PLATFORM=windows ;;
  *) echo "error: run on macOS or in an MSYS2 MINGW64 shell" >&2; exit 2 ;;
esac

# --- throwaway clone at <ref> -------------------------------------------------
rm -rf "$REL"
git clone -q "$SRC" "$REL"
git -C "$REL" checkout -q --detach "$SHA"
for sm in psxrecomp recomp-ui; do
  git -C "$REL" config "submodule.$sm.url" "$SRC/$sm"
done
git -C "$REL" submodule update -q --init
git -C "$REL/psxrecomp" submodule update -q --init --recursive
cd "$REL"

V="$(tr -d '[:space:]' < VERSION)"; V="${V#v}"
export RELEASE_VERSION="$V"
export EXCLUDE_DEV_MODS=1
echo "== r4 $V ($PLATFORM) from $SHA"
bash psxrecomp/tools/ci/record_pins.sh
bash psxrecomp/tools/ci/clear_generated.sh

EMIT=psxrecomp/recompiler/build
HOST=build-host
EMIT_ARGS=(-DCMAKE_BUILD_TYPE=Release)
HOST_ARGS=(
  -DCMAKE_BUILD_TYPE=Release
  -DPSXRECOMP_FORCE_SETUP_HOST=ON
  -DPSXRECOMP_ALLOW_NO_BIOS=ON
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
  MAP="-ffile-prefix-map=$REL/=./"
  EMIT_ARGS+=("${MAC[@]}" "-DCMAKE_C_FLAGS=$MAP" "-DCMAKE_CXX_FLAGS=$MAP")
  HOST_ARGS+=("${MAC[@]}" "-DCMAKE_C_FLAGS=$MAP" "-DCMAKE_CXX_FLAGS=$MAP"
              -DCMAKE_DISABLE_FIND_PACKAGE_SDL3=ON
              -DCMAKE_DISABLE_FIND_PACKAGE_Freetype=ON
              -DSDL_HIDAPI_LIBUSB=OFF)
  EXE=r4-runtime; SFX=""
else
  # Static emitters, so a player's PATH can never hand them the wrong runtime
  # DLLs; the host links SDL3/zlib/libstdc++ statically already.
  EMIT_ARGS+=(-DPSXRECOMP_STATIC_CLI=ON)
  unset TOOLCHAIN_DIR PSXRECOMP_TOOLCHAIN_DIR RETCOMM_TOOLCHAIN_DIR BPE_TOOLCHAIN_DIR || true
  export PSXRECOMP_RUNTIME_BIN_DIR=/mingw64/bin
  EXE=r4-runtime.exe; SFX=.exe
fi

cmake -S psxrecomp/recompiler -B "$EMIT" -G Ninja "${EMIT_ARGS[@]}" >/dev/null
cmake --build "$EMIT" --target psxrecomp-game psxrecomp-bios -j"$JOBS" >/dev/null
cmake -S . -B "$HOST" -G Ninja "${HOST_ARGS[@]}" >/dev/null
cmake --build "$HOST" --target psx-runtime -j"$JOBS" >/dev/null

# --- gates: portable binaries, right version ----------------------------------
test "$(tr -d '[:space:]' < "$HOST/psx_game_version.txt")" = "$V"
BINS=("$HOST/$EXE" "$EMIT/psxrecomp-game$SFX" "$EMIT/psxrecomp-bios$SFX")
for b in "${BINS[@]}"; do
  if [[ $PLATFORM == macos ]]; then
    [[ "$(lipo -archs "$b")" == "x86_64 arm64" ]] || { echo "not universal: $b" >&2; exit 1; }
    if otool -L "$b" | tail -n +2 | grep -vE '^[[:space:]]+(/usr/lib/|/System/Library/)'; then
      echo "non-system dylib in $b" >&2; exit 1
    fi
    [[ "$(otool -l "$b" | awk '/LC_BUILD_VERSION/{f=1} f&&/minos/{print $2; f=0}' | sort -u)" == "11.0" ]] \
      || { echo "minos is not 11.0: $b" >&2; exit 1; }
    ! strings "$b" | grep -qF "$REL" || { echo "build path leaked into $b" >&2; exit 1; }
  else
    if objdump -p "$b" | awk '/DLL Name:/{print tolower($3)}' \
        | grep -vE '^(kernel32|user32|gdi32|shell32|advapi32|ole32|oleaut32|comdlg32|comctl32|imm32|winmm|version|setupapi|cfgmgr32|hid|dinput8|dxgi|d3d11|d3d12|dwrite|d2d1|windowscodecs|opengl32|ws2_32|iphlpapi|bcrypt|crypt32|secur32|userenv|dbghelp|shlwapi|ntdll|uxtheme|dwmapi|mfplat|mfreadwrite|mf|propsys|api-ms-win-.*|ucrtbase|msvcrt)\.dll$'; then
      echo "non-system DLL import in $b" >&2; exit 1
    fi
  fi
done

# --- sign (macOS: ad-hoc; no Developer ID, which would embed a personal name) -
if [[ $PLATFORM == macos ]]; then
  for b in "${BINS[@]}"; do codesign --force --sign - "$b"; done
fi

# --- package -------------------------------------------------------------------
if ! command -v zip >/dev/null 2>&1; then
  # The packager only runs `zip -r -q <out> .`; MSYS2 does not ship zip by default.
  mkdir -p .tools
  cat > .tools/zip <<'PY'
#!/usr/bin/env python3
import os, sys, zipfile
args = [a for a in sys.argv[1:] if not a.startswith("-")]
out, root = args[0], args[1]
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
    for d, _, files in os.walk(root):
        for f in files:
            p = os.path.join(d, f)
            z.write(p, os.path.relpath(p, root).replace(os.sep, "/"))
PY
  chmod +x .tools/zip
  export PATH="$PWD/.tools:$PATH"
fi
if [[ $PLATFORM == macos ]]; then ARTS=(macos-arm64 macos-x64); else ARTS=(windows-x64); fi
for A in "${ARTS[@]}"; do
  scripts/package_setup_release.sh "$HOST" "$A" "$EMIT" >/dev/null
done

# --- verify each zip -------------------------------------------------------------
for A in "${ARTS[@]}"; do
  Z="dist/r4-$V-$A.zip"
  L="$(python3 -c 'import sys,zipfile; print("\n".join(zipfile.ZipFile(sys.argv[1]).namelist()))' "$Z")"
  if printf '%s\n' "$L" | grep -E '^(generated|disc|saves|ghidra|renderer|tools|build[^/]*|cache)/|^psxrecomp/generated/|SLUS_007\.97_|(^|/)SCPH[0-9]+\.(BIN|bin)$|\.(cue|iso|chd|img|ccd|sub)$|(^|/)(settings\.toml|bios\.cfg|disc\.cfg|overlay_captures\.json)$|^mods/installed/'; then
    echo "forbidden entries above in $Z" >&2; exit 1
  fi
  bins="$(printf '%s\n' "$L" | grep -iE '\.bin$' | grep -vxF psxrecomp/bios/openbios.bin || true)"
  [[ -z "$bins" ]] || { echo "unexpected .bin in $Z: $bins" >&2; exit 1; }
  for f in "$EXE" "psxrecomp/recompiler/build/psxrecomp-game$SFX" "psxrecomp/recompiler/build/psxrecomp-bios$SFX" \
           psx_game_version.txt VERSION LICENSE DISC.md CMakeLists.txt game.toml codegen_setup.c \
           seeds/ghidra_funcs.txt annotations/SLUS_007.97_annotations.csv psxrecomp/psxrecomp_cli.py \
           psxrecomp/bios/openbios.bin psxrecomp/bios/OpenBIOS.LICENSE psxrecomp/LICENSE recomp-ui/LICENSE; do
    printf '%s\n' "$L" | grep -qxF "$f" || { echo "missing $f in $Z" >&2; exit 1; }
  done
  (cd dist && shasum -a 256 "r4-$V-$A.zip" 2>/dev/null || sha256sum "r4-$V-$A.zip") > "dist/r4-$V-$A.zip.sha256"
  echo "OK $Z ($(du -h "$Z" | cut -f1))"
done
echo "zips in $REL/dist"

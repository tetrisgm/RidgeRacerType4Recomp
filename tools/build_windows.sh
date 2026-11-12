#!/usr/bin/env bash
# Cross-compile on macOS and stage an immutable portable owner playtest build.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
[[ "$(uname -s)" == Darwin ]] || { echo 'Run this on the Mac.' >&2; exit 2; }
[[ $# == 1 && "$1" =~ ^[A-Za-z0-9][A-Za-z0-9._-]*$ ]] || { echo 'Usage: tools/build_windows.sh VERSION' >&2; exit 2; }
VERSION="$1"
[[ ! -e "$ROOT/dist/windows/$VERSION" ]] || { echo 'Version already exists; choose a new one.' >&2; exit 2; }
export LLVM_MINGW_ROOT="${LLVM_MINGW_ROOT:-$HOME/.local/toolchains/llvm-mingw-20261104-ucrt-macos-universal}"
[[ -x "$LLVM_MINGW_ROOT/bin/x86_64-w64-mingw32-clang" ]] || { echo 'Install the pinned llvm-mingw toolchain: docs/WINDOWS_BUILD.md' >&2; exit 2; }
cmake -S "$ROOT" -B "$ROOT/build-windows" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$ROOT/tools/toolchain-windows-llvm-mingw.cmake" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS='-include exception' \
  -DR4_BUILD_TESTS=OFF -DR4_PGXP=ON -DPSX_DEBUG_TOOLS=OFF \
  -DPSX_EXECUTION_PROFILE=ENHANCED -DPSX_RUNTIME_IPO=ON \
  -DPSX_STATIC_RUNTIME=ON -DPSX_ENABLE_VULKAN=OFF -DPSXRECOMP_BIOS_STEMS=OpenBIOS \
  -DCMAKE_DISABLE_FIND_PACKAGE_Freetype=ON -DCMAKE_DISABLE_FIND_PACKAGE_SDL3=ON
cmake --build "$ROOT/build-windows" --parallel "${JOBS:-2}"
python3 "$ROOT/tools/stage_windows.py" "$VERSION" --toolchain "$LLVM_MINGW_ROOT" --race-60

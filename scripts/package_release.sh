#!/usr/bin/env bash
# Thin wrapper around the shared psxrecomp bundled-release packager, from
# psxrecomp's tools/new_project_layout/templates/package_release.sh.in.
# tools/package_release.sh builds the binaries and calls this; run it directly
# only on a build configured the same way.
#
# Ships the COMPILED game built from this repo's committed generated/ C:
# executable, runtime data, bundled OpenBIOS, mod catalog, overlay toolchain
# (overlay_toolchain/: the two emitters, runtime headers, a Python runtime),
# third-party notices. No R4 or framework sources, no generated C, no CLI, no
# emitters at the root, no disc data, no BIOS image other than OpenBIOS.
#
# Usage:
#   scripts/package_release.sh <build-dir> <artifact-tag> [recompiler-build-dir]
#
# Writes: dist/r4-<VERSION>-<artifact-tag>.zip
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${1:-}"
ARTIFACT_TAG="${2:-}"
RECOMPILER_BUILD="${3:-build-recompiler}"

if [[ -z "${BUILD_DIR}" || -z "${ARTIFACT_TAG}" ]]; then
  echo "usage: $0 <build-dir> <artifact-tag> [recompiler-build-dir]" >&2
  exit 2
fi

PACKAGER="${ROOT}/psxrecomp/tools/package_game_release.sh"
if [[ ! -f "${PACKAGER}" ]]; then
  echo "error: missing ${PACKAGER} (psxrecomp submodule predates bundled releases -- bump it)" >&2
  exit 1
fi
chmod +x "${PACKAGER}" 2>/dev/null || true

EXTRA=()
# Overlay shard cache. It is compiled FROM THE DISC, so none ships: R4 has no
# static AOT shard (ISSUES.md #4), and the bundled overlay_toolchain/ compiles
# each R4.BIN code overlay from the player's disc on its first visit. A shard
# cache is game code compiled from the disc; do not ship one.
if [[ -n "${PSX_OVERLAY_CACHE_ROOT:-}" ]]; then
  EXTRA+=(--overlay-cache-root "${PSX_OVERLAY_CACHE_ROOT}")
else
  EXTRA+=(--ship-without-overlay-cache-because \
    "R4 has no static AOT shard; overlay_toolchain/ compiles each code overlay from the player's disc on first visit")
fi
# Docs at the zip root: DISC.md tells players which dump works; LICENSE covers
# R4's own code in the executable (its mod plugins). Release notes go on the
# release page, not in the zip.
for _doc in DISC.md LICENSE; do
  if [[ -f "${ROOT}/${_doc}" ]]; then
    EXTRA+=(--doc "${_doc}")
  fi
done
# Third-party notices (box art credit, MegaManX6Recomp), path kept.
EXTRA+=(--runtime-file THIRD-PARTY-LICENSES/README.md)
# recomp-ui's license and asset notices, and TinyCC's license in the Windows
# overlay_toolchain/tcc/, are staged by the framework packager itself.
if [[ -d "${ROOT}/${BUILD_DIR}" ]]; then EXE_DIR="${ROOT}/${BUILD_DIR}"; else EXE_DIR="${BUILD_DIR}"; fi
mkdir -p "${EXE_DIR}/licenses"
# Statically linked libraries whose licenses (MIT) ask for their notice in
# binary copies, and that no framework packager stages: Dear ImGui (recomp-ui's
# launcher UI), recomp-net and retcomm-rbengine (netplay).
# tools/package_release.sh requires all three in every zip.
NOTICES=(
  "recomp-ui/src/third_party/imgui/LICENSE.txt|dear-imgui-LICENSE.txt"
  "psxrecomp/lib/recomp-net/LICENSE|recomp-net-LICENSE"
  "psxrecomp/lib/retcomm-rbengine/LICENSE|retcomm-rbengine-LICENSE"
)
# Windows: the MinGW-w64 C runtime startup code and winpthreads are linked
# statically into r4-runtime.exe and both emitters; winpthreads' license asks
# for its notice in binary copies. Taken from the MSYS2 packages they were
# linked from.
case "${ARTIFACT_TAG}" in
  windows-*)
    MSYS_LIC="${MINGW_PREFIX:-/mingw64}/share/licenses"
    NOTICES+=("${MSYS_LIC}/winpthreads/COPYING|winpthreads-COPYING"
              "${MSYS_LIC}/crt/COPYING.MinGW-w64-runtime.txt|mingw-w64-runtime-COPYING.txt")
    ;;
esac
for _n in "${NOTICES[@]}"; do
  _src="${_n%%|*}"; _dst="${_n##*|}"
  [[ "${_src}" == /* ]] || _src="${ROOT}/${_src}"
  [[ -f "${_src}" ]] || { echo "error: missing notice ${_src}" >&2; exit 1; }
  cp "${_src}" "${EXE_DIR}/licenses/${_dst}"
  EXTRA+=(--runtime-file "licenses/${_dst}")
done
cd "${ROOT}"
exec bash "${PACKAGER}" \
  --root "${ROOT}" \
  --build-dir "${BUILD_DIR}" \
  --artifact "${ARTIFACT_TAG}" \
  --zip-prefix r4 \
  --exe-name r4-runtime \
  --display-name "Ridge Racer Type 4 Recompiled" \
  --recompiler-build "${RECOMPILER_BUILD}" \
  --version-env RELEASE_VERSION \
  --disc-hint "your own R4: Ridge Racer Type 4 (USA) disc image (Redump bin/cue, see DISC.md)" \
  "${EXTRA[@]}"

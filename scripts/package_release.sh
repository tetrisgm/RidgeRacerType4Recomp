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
# recomp-ui's notices. r4-runtime links recomp-ui (MIT) and ships its fonts and
# flag sheet (OFL-1.1, CC BY-SA 4.0), but the framework packager stages only
# psxrecomp's own notices. Put recomp-ui's beside the exe: the NOTICE.md files
# travel with assets/, the license goes in as licenses/recomp-ui-LICENSE.
# tools/package_release.sh requires all three in every zip.
if [[ -d "${ROOT}/${BUILD_DIR}" ]]; then EXE_DIR="${ROOT}/${BUILD_DIR}"; else EXE_DIR="${BUILD_DIR}"; fi
UI="${ROOT}/recomp-ui"
for _f in LICENSE assets/common/fonts/NOTICE.md assets/common/img/NOTICE.md; do
  [[ -f "${UI}/${_f}" ]] || { echo "error: missing recomp-ui/${_f}" >&2; exit 1; }
done
for _d in assets/fonts assets/img; do
  [[ -d "${EXE_DIR}/${_d}" ]] || { echo "error: ${EXE_DIR}/${_d} missing -- build psx-runtime first" >&2; exit 1; }
done
mkdir -p "${EXE_DIR}/licenses"
cp "${UI}/LICENSE" "${EXE_DIR}/licenses/recomp-ui-LICENSE"
cp "${UI}/assets/common/fonts/NOTICE.md" "${EXE_DIR}/assets/fonts/NOTICE.md"
cp "${UI}/assets/common/img/NOTICE.md" "${EXE_DIR}/assets/img/NOTICE.md"
EXTRA+=(--runtime-file licenses/recomp-ui-LICENSE)
# TinyCC's license. The Windows overlay toolchain ships TinyCC (LGPL-2.1) in
# overlay_toolchain/tcc/, but the pinned tcc-0.9.27-win64-bin.zip has no
# license file and the framework packager adds none. Stage the LGPL-2.1 text
# there as COPYING before the toolchain is staged (it only adds to that
# folder); tools/package_release.sh requires it in the Windows zip. Drop this
# at the psxrecomp pin whose packager ships TinyCC's license itself.
case "${ARTIFACT_TAG}" in
  windows-*)
    TCC_LICENSE="${ROOT}/THIRD-PARTY-LICENSES/TinyCC-LGPL-2.1.txt"
    [[ -f "${TCC_LICENSE}" ]] || { echo "error: missing ${TCC_LICENSE}" >&2; exit 1; }
    mkdir -p "${EXE_DIR}/overlay_toolchain/tcc"
    cp "${TCC_LICENSE}" "${EXE_DIR}/overlay_toolchain/tcc/COPYING"
    EXTRA+=(--runtime-file overlay_toolchain/tcc/COPYING)
    ;;
esac

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

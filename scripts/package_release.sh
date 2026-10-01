#!/usr/bin/env bash
# Thin wrapper around the shared psxrecomp bundled-release packager, from
# psxrecomp's tools/new_project_layout/templates/package_release.sh.in.
# tools/package_release.sh builds the binaries and calls this; run it directly
# only on a build configured the same way.
#
# Ships the COMPILED game built from this repo's committed generated/ C:
# executable, runtime data, bundled OpenBIOS, mod catalog, overlay toolchain.
# No sources, emitters, CLI, generated C, or BIOS dumps.
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

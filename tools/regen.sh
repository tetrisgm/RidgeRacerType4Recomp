#!/usr/bin/env bash
# Regenerate the game C (generated/SLUS_007.97_*.c) from the disc. generated/
# is committed: releases are built from it, so commit the result.
# Required after changing seeds/, annotations/, [recompiler] config, or the
# psxrecomp submodule. Never hand-edit generated/.
#
#   tools/regen.sh [--disc path/to/R4.cue] [extra psxrecomp_cli generate args]
#
# The recompiled BIOS backends (OpenBIOS, SCPH-1001) are committed in the
# psxrecomp submodule and move with its pin; this script never rewrites them.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
# Keep the CLI's emitter build under the requested job limit.
JOBS="${JOBS:-2}"
export CMAKE_BUILD_PARALLEL_LEVEL="$JOBS"
# Generate already rebuilds build-recompiler/ incrementally when sources are
# present. A second shell build used a differently formatted toolchain stamp
# on Windows and made the CLI discard and rebuild those same objects.
"${PYTHON:-python3}" psxrecomp/psxrecomp_cli.py generate --config game.toml --project-root . \
    --no-toolchain-download "$@"
# GEN_FULL_GLOB is evaluated at configure time, so re-configure existing build
# trees in case the shard count changed.
# Only trees configured from this project root (not old recompiler trees,
# which point elsewhere).
for cache in build*/CMakeCache.txt; do
    [[ -f "$cache" ]] || continue
    home="$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$cache")"
    [[ "$home" -ef "$ROOT" ]] || continue
    cmake -S . -B "$(dirname "$cache")" >/dev/null
done

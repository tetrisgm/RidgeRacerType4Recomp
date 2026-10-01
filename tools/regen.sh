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
# Emitters in the project's build-recompiler/ (gitignored): the tree the
# framework CLI prefers and the one tools/run_r4.sh compiles overlays with.
# Always (re)built incrementally, so a pin bump never generates with emitters
# from the previous pin.
bash psxrecomp/tools/ci/build_emitters.sh --framework psxrecomp \
    --build-dir build-recompiler >/dev/null
python3 psxrecomp/psxrecomp_cli.py generate --config game.toml --project-root . \
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

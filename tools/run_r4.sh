#!/usr/bin/env bash
# Launch a local build straight into the game (no launcher).
#
#   tools/run_r4.sh [build-dir] [extra runtime args]
# Default build dir: build (configure with -DPSX_DEBUG_TOOLS=ON for the TCP
# debug server on port 4797 that tools/dbg.py and tools/pad.py drive).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${1:-build}"
[[ $# -gt 0 ]] && shift
DISC="$ROOT/disc/R4 - Ridge Racer Type 4 (USA).cue"
# Dev overlay compiles (R4.BIN code overlays -> native shards) with the local
# emitters in build-recompiler/ and the system compiler. Release zips ship
# overlay_toolchain/ beside the exe and the runtime builds its own command from
# it, so this lives here, not in game.toml. Runs with cwd = project root.
if [[ -z "${PSX_OVERLAY_AUTOCOMPILE_CMD:-}" && ! -x "$ROOT/build-recompiler/psxrecomp-game" ]]; then
    echo "note: no build-recompiler/psxrecomp-game, so R4.BIN overlays stay interpreted;" \
         "build it with: bash psxrecomp/tools/ci/build_emitters.sh --framework psxrecomp --build-dir build-recompiler" >&2
fi
: "${PSX_OVERLAY_AUTOCOMPILE_CMD:=python3 psxrecomp/tools/compile_overlays.py --game-toml game.toml --recompiler build-recompiler/psxrecomp-game --runtime-include psxrecomp/runtime/include --cps}"
export PSX_OVERLAY_AUTOCOMPILE_CMD
exec "$ROOT/$BUILD/r4-runtime" --game "$ROOT/$BUILD/game.toml" --disc "$DISC" --no-launcher "$@"

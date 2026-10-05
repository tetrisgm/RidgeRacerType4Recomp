#!/usr/bin/env bash
# Opt-in three/four-seat Link Battle through the runtime's existing launcher.
# Usage: tools/launch_link_experimental.sh [build-dir] [runtime args...]
# The launcher NETPLAY page already offers Max Players 2, 3, or 4.
set -euo pipefail

R4_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
R4_BUILD="${1:-build}"
if [[ $# -gt 0 ]]; then shift; fi
R4_DISC="$R4_ROOT/disc/R4 - Ridge Racer Type 4 (USA).cue"
if [[ "$R4_BUILD" == /* ]]; then
  R4_BUILD_DIR="$R4_BUILD"
else
  R4_BUILD_DIR="$R4_ROOT/$R4_BUILD"
fi

export PSX_R4_LINK_EXPERIMENTAL=1
export PSX_R4_VIEW_COUNT_PROBE=1
R4_FORCE_LAUNCHER=1
for arg in "$@"; do
  case "$arg" in
    --launcher|--no-launcher|--headless) R4_FORCE_LAUNCHER=0; break ;;
  esac
done
cd "$R4_ROOT"
if [[ "$R4_FORCE_LAUNCHER" == 1 ]]; then
  exec "$R4_BUILD_DIR/r4-runtime" --game "$R4_BUILD_DIR/game.toml" --disc "$R4_DISC" --launcher "$@"
fi
exec "$R4_BUILD_DIR/r4-runtime" --game "$R4_BUILD_DIR/game.toml" --disc "$R4_DISC" "$@"

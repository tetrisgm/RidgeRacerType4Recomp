#!/usr/bin/env bash
# Regenerate OpenBIOS + game C (generated/SLUS_007.97_*.c) from the disc.
# Required after changing seeds/, annotations/, [recompiler] config, or the
# psxrecomp submodule. Never hand-edit generated/.
#
#   tools/regen.sh [--disc path/to/R4.cue] [extra psxrecomp_cli generate args]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
if [[ ! -x build-recompiler/psxrecomp-game || ! -x build-recompiler/psxrecomp-bios ]]; then
    bash psxrecomp/tools/ci/build_emitters.sh --framework psxrecomp --build-dir build-recompiler
fi
# OpenBIOS through the framework's canonical script: unlike the CLI's own BIOS
# step it records the emitter fingerprint runtime.cmake checks for staleness.
(cd psxrecomp && PSXRECOMP_BIOS_BUILD=../build-recompiler \
    bash tools/regen_bios.sh --config bios/OpenBIOS.toml >/dev/null)
python3 psxrecomp/psxrecomp_cli.py generate --config game.toml --project-root . \
    --no-toolchain-download "$@"
# GEN_FULL_GLOB is evaluated at configure time, so re-configure existing build
# trees in case the shard count changed.
for cache in build*/CMakeCache.txt; do
    [[ -f "$cache" ]] || continue
    case "$cache" in build-recompiler/*|build-adaptive/*) continue ;; esac
    cmake -S . -B "$(dirname "$cache")" >/dev/null
done

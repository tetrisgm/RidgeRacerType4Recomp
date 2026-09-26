#!/usr/bin/env bash
# Regenerate OpenBIOS + game C (generated/SLUS_007.97_*.c) from the disc.
# Required after changing seeds/, annotations/, [recompiler] config, or the
# psxrecomp submodule. Never hand-edit generated/.
#
#   tools/regen.sh [--disc path/to/R4.cue] [extra psxrecomp_cli generate args]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
# Emitters at the framework's default location (gitignored inside psxrecomp/),
# which is also where the setup-host zip ships them and where game.toml's
# overlay_autocompile_cmd looks.
EMIT=psxrecomp/recompiler/build
if [[ ! -x $EMIT/psxrecomp-game || ! -x $EMIT/psxrecomp-bios ]]; then
    bash psxrecomp/tools/ci/build_emitters.sh --framework psxrecomp --build-dir $EMIT
fi
# OpenBIOS through the framework's canonical script: unlike the CLI's own BIOS
# step it records the emitter fingerprint runtime.cmake checks for staleness.
(cd psxrecomp && bash tools/regen_bios.sh --config bios/OpenBIOS.toml >/dev/null)
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

#!/usr/bin/env bash
# Materialize a patched psxrecomp copy carrying the MMX6 adaptive renderer and,
# with --configure, build R4 against it in a separate tree. The stock
# psxrecomp submodule, generated/ and the default 4:3 build are never touched.
#
#   renderer/adaptive/materialize.sh [--configure]
#
# Outputs (all gitignored):
#   build-adaptive/psxrecomp/   patched framework copy (own git repo)
#   generated-adaptive/         game C emitted by the patched recompiler
#   build-adaptive/build/       R4 runtime configured with -DR4_ADAPTIVE_RENDERER=ON
#
# Env overrides: FW_SRC (stock framework checkout), DST (patched copy),
# BUILD_DIR (adaptive build dir), GEN_DIR (adaptive generated-C dir).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
HERE="$ROOT/renderer/adaptive"
FW_SRC="${FW_SRC:-$ROOT/psxrecomp}"
DST="${DST:-$ROOT/build-adaptive/psxrecomp}"
BUILD_DIR="${BUILD_DIR:-$ROOT/build-adaptive/build}"
GEN_DIR="${GEN_DIR:-generated-adaptive}"
BASE="$(sed -n 's/^base[[:space:]]*=[[:space:]]*//p' "$HERE/BASE")"
PATCH_DIR="$HERE/framework-patches"

actual="$(git -C "$FW_SRC" rev-parse HEAD)"
if [[ "$actual" != "$BASE" ]]; then
    echo "framework is at $actual; the patch is verified against $BASE" >&2
    echo "re-verify with: git -C \"$FW_SRC\" apply --check $PATCH_DIR/*.patch" >&2
    [[ "${ALLOW_BASE_MISMATCH:-0}" == 1 ]] || exit 1
fi
if [[ -n "$(git -C "$FW_SRC" status --porcelain --untracked-files=no)" ]]; then
    echo "framework checkout $FW_SRC has tracked changes; refusing to copy" >&2
    exit 1
fi

rm -rf "$DST"
mkdir -p "$DST"
# Working-tree copy including nested submodules (recomp-net, rbengine); every
# .git entry and the stock BIOS/game build products are excluded so the copy
# never aliases the source repo or reuses stale codegen.
rsync -a --exclude='.git' --exclude='__pycache__' --exclude='/generated/' \
    --exclude='/recompiler/build*/' --exclude='/runtime/include/overlay_codegen_hash.h' \
    "$FW_SRC/" "$DST/"
# Own repo so git apply resolves paths inside $DST, never the enclosing repo.
git -C "$DST" init -q
for p in "$PATCH_DIR"/*.patch; do
    git -C "$DST" apply --check "$p"
    git -C "$DST" apply "$p"
done
test -f "$DST/runtime/include/ws_view_anchor.h"
grep -q 'PSX_OVERLAY_ABI_VERSION 24' "$DST/runtime/include/overlay_api.h"
echo "patched framework: $DST (base $BASE)"

if [[ "${1:-}" == "--configure" ]]; then
    # The patch changes codegen (function-filter entry hooks, bias_lower_sites,
    # emitter fingerprint), so BIOS and game C for this build must come from
    # the patched recompiler.
    cmake -S "$DST/recompiler" -B "$DST/recompiler/build" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release
    cmake --build "$DST/recompiler/build" --target psxrecomp-game psxrecomp-bios
    (cd "$DST" && bash tools/regen_bios.sh --config bios/OpenBIOS.toml)
    # Same game config, generated into $GEN_DIR, and overlay shards compiled by
    # the PATCHED recompiler against the patched headers (overlay ABI 24): an
    # ABI-23 shard from the stock toolchain would be rejected at load time.
    sed -e "s#^out_dir = \"generated\"#out_dir = \"$GEN_DIR\"#" \
        -e "s#--recompiler psxrecomp/recompiler/build/psxrecomp-game#--recompiler build-adaptive/psxrecomp/recompiler/build/psxrecomp-game#" \
        -e "s#--runtime-include psxrecomp/runtime/include#--runtime-include build-adaptive/psxrecomp/runtime/include#" \
        -e "s#python3 psxrecomp/tools/compile_overlays.py#python3 build-adaptive/psxrecomp/tools/compile_overlays.py#" \
        "$ROOT/game.toml" > "$ROOT/build-adaptive/game.adaptive.toml"
    (cd "$ROOT" && "$DST/recompiler/build/psxrecomp-game" \
        --config build-adaptive/game.adaptive.toml --project-root "$ROOT")
    cmake -S "$ROOT" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DPSX_DEBUG_TOOLS=ON -DPSXRECOMP_ROOT="$DST" \
        -DR4_GENERATED_DIR="$GEN_DIR" -DR4_ADAPTIVE_RENDERER=ON
    echo "configured: cmake --build \"$BUILD_DIR\" --target psx-runtime"
    echo "run with:   \"$BUILD_DIR/r4-runtime\" --game \"$ROOT/build-adaptive/game.adaptive.toml\""
fi

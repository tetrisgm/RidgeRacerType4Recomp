#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SELECT="$ROOT/tools/select_python.sh"
GOOD="$(bash "$SELECT")"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

ln -s "$GOOD" "$TMP/python3"
ln -s "$GOOD" "$TMP/python3.14"
[[ "$(PATH="$TMP" /bin/bash "$SELECT")" == "$TMP/python3" ]]

rm "$TMP/python3"
printf '#!/bin/sh\nexit 1\n' > "$TMP/python3"
chmod +x "$TMP/python3"
[[ "$(PATH="$TMP" /bin/bash "$SELECT")" == "$TMP/python3.14" ]]
[[ "$(R4_PYTHON="$TMP/python3.14" PATH="$TMP" /bin/bash "$SELECT")" == "$TMP/python3.14" ]]
if R4_PYTHON="$TMP/python3" PATH="$TMP" /bin/bash "$SELECT" >/dev/null 2>&1; then
    echo "invalid R4_PYTHON was accepted" >&2
    exit 1
fi

echo "select_python: PASS"

#!/usr/bin/env bash
# Print a Python with tomllib for R4 tools. Keep the shell's
# python3 when it works; older macOS installations need a versioned Python.
set -euo pipefail

supports_tomllib() {
    command -v "$1" >/dev/null 2>&1 && "$1" -c 'import tomllib' >/dev/null 2>&1
}

if [[ -n ${R4_PYTHON:-} ]]; then
    if supports_tomllib "$R4_PYTHON"; then
        command -v "$R4_PYTHON"
        exit 0
    fi
    echo "error: R4_PYTHON must name a Python 3.11+ interpreter with tomllib" >&2
    exit 2
fi

for candidate in python3 python3.14 python3.13 python3.12 python3.11; do
    if supports_tomllib "$candidate"; then
        command -v "$candidate"
        exit 0
    fi
done

echo "error: R4 tools need Python 3.11+ with tomllib" >&2
exit 2

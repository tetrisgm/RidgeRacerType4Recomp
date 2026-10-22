#!/bin/sh
# r4_screen_run.sh NAME SECONDS [ENV=VAL ...] - launch build/ fullscreen from
# the race save (slot $SLOT, default 5, under $SAVES), drive it with
# r4_autodrive.py and count distinct on-screen frames with screen_frames.exe.
# Results: $OUT/NAME.{txt,csv,log}. Needs a debug-tools build in $BUILD.
cd "$(dirname "$0")/.."
NAME=$1; SECS=$2; shift 2
BUILD=${BUILD:-build}; SLOT=${SLOT:-5}; OUT=${OUT:-analysis/screen}; SAVES=${SAVES:-$(pwd)/analysis/screen/saves}
PY=/c/Users/Shokunin/AppData/Local/Programs/Python/Python312/python.exe
mkdir -p "$OUT"
export PATH="/c/msys64/mingw64/bin:$PATH" PSX_BIOS_HLE=0 PSX_OVERLAY_BACKEND=gcc \
       PSX_OVERLAY_AUTOCOMPILE_OFF=1 PSX_LOAD_SLOT=$SLOT PSX_FRAME_REPORT_MS=500
for kv in "$@"; do export "$kv"; done
"./$BUILD/r4-runtime.exe" --game "$BUILD/game.toml" --disc "disc/R4 - Ridge Racer Type 4 (USA).cue" \
  --no-launcher --bios psxrecomp/bios/openbios.bin --memcard-dir "$(cygpath -w "$SAVES")" \
  --debug-port 4797 > "$OUT/$NAME.log" 2>&1 &
sleep 1
grep -a -q "LOAD FAILED" "$OUT/$NAME.log" && echo "save did not load"
./tools/screen_frames.exe "$SECS" "$(cygpath -w "$OUT/$NAME.csv")" > "$OUT/$NAME.txt" &
CAP=$!
sleep 3

(cd tools && "$PY" r4_autodrive.py $((SECS - 4)) > "../$OUT/$NAME.drive" 2>&1)
wait $CAP
powershell -NoProfile -c "Get-Process r4-runtime -EA 0 | Stop-Process -Force"
grep -a -c "LOAD FAILED" "$OUT/$NAME.log" | sed 's/^/load_failed=/' >> "$OUT/$NAME.txt"
cat "$OUT/$NAME.txt"

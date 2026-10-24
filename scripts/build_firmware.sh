#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${ZY_BUILD_ROOT:-$ROOT/build}/zynaps-devops"
python3 "$ROOT/scripts/prepare_firmware.py" "$OUT"
export BOARD_TYPE=pico_w DEBUG_MODE=0
export RELEASE_VERSION="$(tr -d '\r\n' < "$ROOT/VERSION")-zynaps-wide"
export RELEASE_DATE=2026-10-23 APP_UUID_KEY=5a595749-4445-4000-8000-000000000001
cmake -S "$OUT/rp/src" -B "$OUT/build" -G Ninja -DCMAKE_BUILD_TYPE=Release
if ! cmake --build "$OUT/build" --parallel 4 > "$OUT/build/build.log" 2>&1; then
    tail -25 "$OUT/build/build.log"
    exit 1
fi
tail -15 "$OUT/build/build.log"
DIST="${ZY_DIST_ROOT:-$ROOT/dist}"
mkdir -p "$DIST"
cp "$OUT/build/rp.uf2" "$DIST/ZYNAPS-SC.uf2"
cp "$OUT/build/rp.elf" "$DIST/ZYNAPS-SC.elf"

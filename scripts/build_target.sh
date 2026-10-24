#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TARGET="$ROOT/build/target"
DEVOPS="${ZY_DEVOPS_ROOT:-$ROOT/build/deps/md-devops}"
[ -d "$DEVOPS" ] || DEVOPS="$ROOT/../vendor/md-devops"
mkdir -p "$TARGET"
cp -R "$DEVOPS/target/atarist/src" "$TARGET/"
cp "$DEVOPS/target/atarist/Makefile" "$TARGET/"
cp "$DEVOPS/target/atarist/version.txt" "$TARGET/"
cp "$DEVOPS/target/atarist/firmware.py" "$TARGET/"
find "$TARGET/src" -type f -exec sed -i 's/\r$//' {} +
sed -i 's/\r$//' "$TARGET/Makefile" "$TARGET/version.txt"
python3 "$ROOT/scripts/patch-target.py" "$TARGET/src"
cd "$TARGET"
make release DEBUG_MODE=0
test "$(wc -c < dist/BOOT.BIN)" -le 10240
cp dist/BOOT.BIN dist/FIRMWARE.IMG
truncate -s 65536 dist/FIRMWARE.IMG
python3 firmware.py --input dist/FIRMWARE.IMG --output target_firmware.h --array_name target_firmware

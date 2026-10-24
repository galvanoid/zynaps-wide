#!/bin/bash
# Run inside the canonical toolkit image (with the freestanding ELF tools).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export ZY_MODE=play ZY_NEXT_FIXES=record,boss,ground,preshift,terrain,pacing
export ZY_VBL1=0 ZY_FULL_GPU=1 ZY_PROFILE=0 ZY_GPU_DISABLE=0
export ZY_ASYNC_BLITTER=0 ZY_SPARSE_EXPERIMENTAL=0 ZY_DIAGNOSTIC=0
export ZY_RECREATE_ROOT="$ROOT/build/app/recreate"
export ZY_BUILD_ROOT="$ROOT/build/app" ZY_DIST_ROOT="$ROOT/build/app/disk"
bash "$ROOT/scripts/build_atari.sh"
python3 "$ROOT/scripts/check-build-flavor.py" "$ZY_DIST_ROOT/ZYNAPS.PRG"
bash "$ROOT/scripts/check-helpers.sh" "$ZY_BUILD_ROOT/atari/ZYNAPS.elf"
mkdir -p "$ROOT/dist"
cp "$ZY_DIST_ROOT/ZYNAPS.PRG" "$ROOT/dist/ZYWIDE.PRG"

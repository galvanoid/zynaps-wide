#!/bin/bash
# What the linker actually reached, which no source review can see.
#
# check-68000.sh reads the hand-written assembly, and it is right to. But a divide
# or a modulo on a value the compiler cannot prove small is not an instruction at
# all - it is a CALL into libgcc.a, which nobody reviews, and the 68000 helpers in
# that archive carry a branch gas filed in a form the linker does not always
# shorten. The archive is in every build, and a build that never calls it is fine,
# so the only question is reachability, and that is a question about the linked
# program. scripts/check-helpers.py answers it from the disassembly.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
elf="${1:-$ROOT/build/fix-25/atari/ZYNAPS.elf}"
[ -f "$elf" ] || { echo "no ELF to read: $elf" >&2; exit 2; }
if python3 "$ROOT/scripts/check-helpers.py" "$elf"; then
    echo "PASS: $elf reaches no incompatible libgcc helper"
else
    echo "$elf: a 68000 could fault inside a helper this program called." >&2
    exit 1
fi

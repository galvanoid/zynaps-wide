"""Which libgcc helpers this program can actually reach, and whether it would fault.

The link drags the 68000 divide, modulo and multiply helpers in from libgcc.a in
every build, shipping and broken alike, and a build that never calls one runs
forever. Presence is therefore not the question. The questions are whether any of
our code branches into a helper, and whether that helper holds a branch gas left
unrelaxed - 61FF, BSR with the displacement filled in as -1. A 68020 reads that as
a 32-bit branch; a plain 68000 reads it as a two-byte branch to the next byte, an
odd address, and faults. Hatari's default CPU is a 68030, so the smoke test passes
and the machine shows three bombs (docs/bughunt-wide.md 15).

So: a helper reached from outside itself is fatal, and the unrelaxed branch inside
an unreached helper is a fact about the archive rather than about this program.

    check-helpers.py build/fix-25/atari/ZYNAPS.elf
"""
import re
import subprocess
import sys

OBJDUMP = "m68k-linux-gnu-objdump"
HELPER = re.compile(r"(__u?divsi3|__u?modsi3|__mulsi3)$")
FUNCTION = re.compile(r"^([0-9a-f]+) <([^>]+)>:$", re.I)
BRANCH = re.compile(r"^\s*([0-9a-f]+):\s*((?:[0-9a-f]{4} )+)(.*)", re.I)
TARGET = re.compile(r"([0-9a-f]{4,8})\s*<([^>+]+)", re.I)
UNRELAXED = re.compile(r"61ff\s+bsr[swl]?\s+([0-9a-f]+)", re.I)
CONTROL = re.compile(r"\b(?:jsr|jmp|bsr(?:[swl]|\.[bwl])?|bra(?:[swl]|\.[bwl])?)\s", re.I)


def disassemble(program):
    text = subprocess.run([OBJDUMP, "-d", program], capture_output=True, text=True, check=True).stdout
    current = "?"
    for line in text.splitlines():
        name = FUNCTION.match(line)
        if name:
            current = name.group(2)
            continue
        yield current, line


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    reached = {}
    faulted = set()
    for current, line in disassemble(sys.argv[1]):
        lazy = UNRELAXED.search(line)
        if lazy:
            address = int(re.match(r"\s*([0-9a-f]+):", line, re.I).group(1), 16)
            if int(lazy.group(1), 16) == address + 1:
                faulted.add(current)
        # objdump labels a rodata address as the nearest preceding symbol.
        # LEA of a filename after __umodsi3 is data, not a call to that helper.
        if not CONTROL.search(line):
            continue
        for jump in TARGET.finditer(line):
            target = jump.group(2).split("+")[0]
            if HELPER.search(target) and target != current:
                reached.setdefault(target, []).append((jump.group(1), current))
    bad = sorted(name for name in reached if name in faulted)
    plain = sorted(name for name in reached if name not in faulted)
    for name in bad:
        for caller, inside in reached[name]:
            print("%s reached from %s at %s, and it holds a branch a 68000 faults on"
                  % (name, inside, caller))
    for name in plain:
        print("%s reached from %s" % (name, ", ".join(sorted({i for _, i in reached[name]}))))
    if faulted and not bad:
        print("note: %s hold unrelaxed branches, and nothing calls them." % ", ".join(sorted(faulted)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())

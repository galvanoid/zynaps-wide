"""Refuse a Zynaps Wide program that was built for the Hatari harness.

ZY_MODE=game is the smoke build: it creates FRAME*.BIN, PAL*.BIN, ENT*.BIN and
STATE.BIN through GEMDOS from inside the frame loop, and it leaves for TOS once
its 300-frame budget is spent. On a physical Atari that is a stutter and a return
to Runner, and the file is the same size as a play build, so build_atari.sh stamps
harness builds with `zy_build_flavor` and this check refuses them. Two reasons are
a reason to stop:

  * the program carries the stamp;
  * the program is as new as the capture dumps sitting beside it, which is what a
    harness run leaves behind. A directory holding FRAME1.BIN and STATE.BIN that
    were written after this program was linked is a run directory, whatever the
    stamp says, and programs built before the stamp existed have none.

Anything else is reported as unmarked and accepted: play builds never carry the
stamp, and only the harness is a reason to stop.
"""
import sys
from pathlib import Path

HARNESS = b"ZYHARNESS"
CAPTURES = ('FRAME?.BIN', 'STATE.BIN', 'SCREEN.BIN')
DEFAULT = Path(__file__).resolve().parents[1] / 'dist' / 'ZYNAPS.PRG'


def newest_capture(program):
    stamps = [d.stat().st_mtime for pattern in CAPTURES for d in program.parent.glob(pattern)]
    return max(stamps) if stamps else None


def check(program):
    data = program.read_bytes()
    if HARNESS in data:
        print(f'{program}: Hatari harness build, {len(data)} bytes -- never launch this on the Atari',
              file=sys.stderr)
        return 1
    stamp = newest_capture(program)
    if stamp is not None and stamp >= program.stat().st_mtime - 1:
        print(f'{program}: capture dumps as new as this program sit beside it, so this is a '
              f'harness run directory -- never launch it on the Atari', file=sys.stderr)
        return 1
    print(f'{program}: no harness stamp, {len(data)} bytes -- accepted')
    return 0


def main(argv):
    paths = [Path(arg) for arg in argv[1:]] or [DEFAULT]
    return sum(check(path) for path in paths)


if __name__ == '__main__':
    sys.exit(main(sys.argv))

"""Reject UF2s that touch Booster, configuration, ROM scratch or non-RP2040 flash."""
import pathlib
import struct
import sys

path = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else 'firmware/artifacts/STNEXT-SC.uf2')
data = path.read_bytes()
assert data and len(data) % 512 == 0, 'Incomplete UF2'
seen = set()
addresses = set()
lowest, highest = 0xFFFFFFFF, 0
for offset in range(0, len(data), 512):
    block = data[offset:offset + 512]
    magic0, magic1, flags, addr, size, number, total, family = struct.unpack_from('<8I', block)
    assert (magic0, magic1) == (0x0A324655, 0x9E5D5157), 'Wrong UF2 magic'
    assert struct.unpack_from('<I', block, 508)[0] == 0x0AB16F30
    assert flags == 0x2000 and family == 0xE48BFF56, 'Not an RP2040 flash UF2'
    assert size == 256 and addr % 256 == 0, 'Invalid payload/alignment'
    assert total == len(data) // 512 and number < total and number not in seen
    assert 0x10000000 <= addr and addr + size <= 0x10092000, 'UF2 enters reserved private cache or protected flash'
    assert addr not in addresses, 'Duplicate flash address'
    seen.add(number)
    addresses.add(addr)
    lowest, highest = min(lowest, addr), max(highest, addr + size)
assert lowest == 0x10000000 and len(seen) == total
print(f'PASS: {total} blocks, flash 0x{lowest:08X}..0x{highest:08X}; Booster/settings untouched')

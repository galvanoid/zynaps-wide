"""Bundle only our clean Atari executable, in independently decoded LZ4 blocks."""
from pathlib import Path
import sys,zlib

def pack(data):
    out=bytearray();table={};anchor=i=0
    def length(n):
        while n>=255:out.append(255);n-=255
        out.append(n)
    while i+4<=len(data):
        key=data[i:i+4];prev=table.get(key,-1);table[key]=i
        if prev<0 or i-prev>65535:i+=1;continue
        end=i+4
        while end<len(data) and data[end]==data[prev+end-i]:end+=1
        literal,match=i-anchor,end-i-4
        out.append((min(literal,15)<<4)|min(match,15))
        if literal>=15:length(literal-15)
        out.extend(data[anchor:i]);out.extend((i-prev).to_bytes(2,'little'))
        if match>=15:length(match-15)
        i=anchor=end
    literal=len(data)-anchor;out.append(min(literal,15)<<4)
    if literal>=15:length(literal-15)
    out.extend(data[anchor:]);return out

def main():
    prg,out=map(Path,sys.argv[1:]);raw=prg.read_bytes()
    assert raw[:2]==b'\x60\x1a' and len(raw)<120000,'Unexpected distribution program'
    blocks=[];blob=bytearray()
    for at in range(0,len(raw),8192):
        block=raw[at:at+8192];packed=pack(block)
        blocks.append((len(blob),len(packed),len(block)));blob.extend(packed)
    out.mkdir(parents=True,exist_ok=True)
    (out/'program_bundle.h').write_text('''#include <stdint.h>
typedef struct {unsigned offset,packed,raw;} zy_program_block;
extern const uint8_t zy_program_data[];
extern const zy_program_block zy_program_index[];
extern const unsigned zy_program_size,zy_program_crc,zy_program_blocks;
''')
    (out/'program_bundle.c').write_text('#include "program_bundle.h"\nconst uint8_t zy_program_data[]={'+','.join(map(str,blob))+'};\nconst zy_program_block zy_program_index[]={'+','.join('{%d,%d,%d}'%b for b in blocks)+'};\nconst unsigned zy_program_size=%d,zy_program_crc=0x%08xu,zy_program_blocks=%d;\n'%(len(raw),zlib.crc32(raw),len(blocks)))
    print('Bundled asset-free PRG: %d -> %d bytes (%d blocks)'%(len(raw),len(blob),len(blocks)))
if __name__=='__main__':main()

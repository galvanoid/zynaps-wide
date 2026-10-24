"""Keep TOS relocation terminators in a complete GEMDRIVE byte pair.

Some GEMDRIVE versions swap only complete words, corrupting an odd final byte.
One zero after the relocation terminator is legal trailing PRG padding; text,
BSS and every relocation offset remain unchanged.
"""
from pathlib import Path
import struct,sys

def pad(data):
    if len(data)<32 or data[:2]!=b'\x60\x1a':raise ValueError('Invalid TOS program')
    text,initialized,_,symbols=struct.unpack_from('>4I',data,2)
    if struct.unpack_from('>H',data,26)[0]:raise ValueError('Expected relocatable program')
    at=28+text+initialized+symbols
    if at+4>len(data):raise ValueError('Missing relocations')
    first=struct.unpack_from('>I',data,at)[0];at+=4
    if first:
        while at<len(data) and data[at]:at+=1
        if at==len(data):raise ValueError('Missing relocation terminator')
        at+=1
    if any(data[at:]):raise ValueError('Unexpected nonzero trailing data')
    return data+b'\0' if len(data)&1 else data

if __name__=='__main__':
    path=Path(sys.argv[1]);raw=path.read_bytes();fixed=pad(raw);path.write_bytes(fixed)
    print('GEMDRIVE word-safe PRG:',len(raw),'->',len(fixed),'bytes')

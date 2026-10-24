"""Generate filenames/sizes/checksums only, never original bytes."""
import argparse
from pathlib import Path
import zlib
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('directory',type=Path);p.add_argument('prg',type=Path)
p.add_argument('--output',type=Path,required=True);a=p.parse_args()
files=sorted(f for f in a.directory.iterdir() if f.suffix.upper() in ('.DAT','.PIC','.MAP','.PI1'))
if len(files)!=61:raise SystemExit('Expected 61 original resource files')
lines=['#include <stdint.h>','typedef struct {const char *name;uint32_t size,crc;} zy_file;',
       f'const uint32_t zy_original_crc=0x{zlib.crc32(a.prg.read_bytes()):08x}u;',
       'const zy_file zy_required_files[61]={']
lines+=['{"%s",%du,0x%08xu},'%(f.name.upper(),f.stat().st_size,zlib.crc32(f.read_bytes())) for f in files]
lines+=['};'];a.output.write_text('\n'.join(lines)+'\n')

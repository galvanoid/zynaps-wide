"""Private pre-publication audit against the owner's disk and generated atlas.

No fixture bytes or snippets are written to the report, only counts and hashes.
"""
from pathlib import Path
import argparse,collections,hashlib,json,math,struct

def entropy(block):
    return -sum(n/len(block)*math.log2(n/len(block)) for n in collections.Counter(block).values())

def flash_payload(uf2):
    assert uf2 and len(uf2)%512==0
    memory=bytearray(b'\xff'*0x92000);seen=set()
    for at in range(0,len(uf2),512):
        m0,m1,flags,address,size,index,total,family=struct.unpack_from('<8I',uf2,at)
        assert (m0,m1,flags,family)==(0x0a324655,0x9e5d5157,0x2000,0xe48bff56)
        assert size==256 and address%256==0 and 0x10000000<=address<=0x10092000-size
        assert index not in seen and total==len(uf2)//512
        assert struct.unpack_from('<I',uf2,at+508)[0]==0x0ab16f30
        seen.add(index);offset=address-0x10000000
        memory[offset:offset+size]=uf2[at+32:at+32+size]
    assert len(seen)==total
    return memory

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--originals',type=Path,required=True)
    p.add_argument('--atlas',type=Path,required=True)
    p.add_argument('--dist',type=Path,default=Path(__file__).resolve().parents[1]/'dist')
    p.add_argument('--report',type=Path,required=True);args=p.parse_args()
    files={'ZYWIDE.PRG':(args.dist/'ZYWIDE.PRG').read_bytes(),
           'ZYNAPS-SC.uf2':bytes(flash_payload((args.dist/'ZYNAPS-SC.uf2').read_bytes()))}
    forbidden=[]
    for f in args.dist.rglob('*'):
        if f.is_file() and (f.suffix.upper() in ('.DAT','.PIC','.MAP','.PI1','.IMG','.ST','.MSA') or f.name in ('ZYNAPS17.PRG','ZYWCACHE.BIN')):forbidden.append(f.name)
    assert not forbidden,('Original/private files in distribution',forbidden)
    chunks=set();resource_files=0
    for f in args.originals.iterdir():
        if f.suffix.upper() not in ('.DAT','.PIC','.MAP','.PI1'):continue
        resource_files+=1;raw=f.read_bytes()
        for at in range(0,len(raw)-127,128):
            b=raw[at:at+128]
            if entropy(b)>=3.5:chunks.add(b)
    atlas=(args.atlas/'atlas.c').read_text()
    assert 'zy_atlas_page' in atlas,'Missing private reference'
    for scene in args.atlas.glob('scene-*.bin'):
        raw=scene.read_bytes()
        for first,last in ((0x19e00,0x19f00),(0x1a800,0x35000),(0x54f00,0x6ec00)):
            for at in range(first,last,256):
                b=raw[at:at+256]
                if entropy(b)>=3.5:chunks.add(b)
    assert resource_files==61 and len(chunks)>100,'Incomplete private audit fixtures'
    for name,data in files.items():
        assert not any(b in data for b in chunks),f'Original graphics block found in {name}'
        assert b'ZYHARNESS' not in data,f'Harness artifact: {name}'
    report={'status':'pass','resource_files_checked':resource_files,'distinct_nontrivial_blocks_checked':len(chunks),
            'uf2_highest_allowed_address':'0x10092000','private_cache_not_distributed':True,
            'limitations':'Block matching is a supplement to the asset-free build and explicit file allowlist, not a legal determination.',
            'sha256':{name:hashlib.sha256((args.dist/name).read_bytes()).hexdigest() for name in files}}
    args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_text(json.dumps(report,indent=2)+'\n')
    print('PASS: asset-free artifacts;',resource_files,'original resources,',len(chunks),'distinct blocks checked; private flash region excluded from UF2')
if __name__=='__main__':main()

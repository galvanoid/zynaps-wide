"""Create public archives from explicit clean lists, after the recorded checks."""
from pathlib import Path
import argparse,hashlib,json,subprocess,sys,tempfile,zipfile
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--candidate',action='store_true',help='Package for hardware testing; publication remains pending')
args=p.parse_args()
status=json.loads((root/'build/DISTRIBUTION-STATUS.json').read_text())
audit=json.loads((root/'build/release-audit.json').read_text())
assert status['local_tests']=='passed' and status['isolated_source_build']=='passed'
assert audit['status']=='pass'
for name in ('ZYWIDE.PRG','ZYNAPS-SC.uf2'):
    digest=hashlib.sha256((root/'dist'/name).read_bytes()).hexdigest()
    assert digest==status['sha256'][name]==audit['sha256'][name],'Artifacts changed after verification'
if not args.candidate and any(status.get(field)!='passed' for field in
        ('hardware_installation','physical_gameplay','hardware_cache_relaunch')):
    raise SystemExit('Physical installation/gameplay checks are still pending; use --candidate for test archives')
out=root/'build/release-candidate' if args.candidate else root/'build/release'
out.mkdir(parents=True,exist_ok=True)
suffix='-candidate' if args.candidate else ''
stem='Zynaps-Wide-'+(root/'VERSION').read_text().strip()+suffix
release=[root/'dist'/name for name in ('ZYWIDE.PRG','ZYNAPS-SC.uf2','SHA256SUMS.txt')]
release += [root/name for name in ('README.md','ORIGINAL-FILES.json','LICENSE','UPSTREAM-LICENSE','FIRMWARE-LICENSE','VALIDATION.md')]
with zipfile.ZipFile(out/(stem+'.zip'),'w',zipfile.ZIP_DEFLATED) as archive:
    for file in release:archive.write(file,file.name)
with tempfile.TemporaryDirectory(prefix='public-source-',dir=root/'build') as tmp:
    source=Path(tmp)/'source'
    subprocess.run([sys.executable,str(root/'scripts/package-source.py'),str(source)],check=True)
    with zipfile.ZipFile(out/(stem+'-source.zip'),'w',zipfile.ZIP_DEFLATED) as archive:
        for file in sorted(source.rglob('*')):
            if file.is_file():archive.write(file,str(file.relative_to(source)))
print('Packaged',stem,'(original resources and private cache excluded)')

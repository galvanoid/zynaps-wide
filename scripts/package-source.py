"""Stage source using an explicit allowlist; never copies private build data."""
from pathlib import Path
import argparse,hashlib,json,shutil
root=Path(__file__).resolve().parents[1]
scripts='''bootstrap.py build.ps1 build.sh build-firmware.ps1 build_atari.sh
build_firmware.sh build_target.sh prepare-wide.py reapply-fixes.py
prepare_firmware.py patch-app-main.py patch-preparation.py patch-target.py pad-prg.py pack-program.py
check-build-flavor.py check-helpers.py check-helpers.sh check-firmware.py
install.py inventory-original.py audit-release.py package-source.py package-release.py publish.py
Dockerfile.atari Dockerfile.test Dockerfile.firmware'''.split()
top=['README.md','LICENSE','UPSTREAM-LICENSE','FIRMWARE-LICENSE','ORIGINAL-FILES.json',
     'VALIDATION.md','STORE-SUBMISSION.md','apps.json','VERSION','.gitignore']
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('output',type=Path);args=parser.parse_args()
out=args.output.resolve()
assert out!=root and root not in out.parents or out.is_relative_to(root/'build')
if out.exists() and any(out.iterdir()):raise SystemExit('Choose a new/empty source output folder; existing files left untouched')
files=[root/p for p in top]+[root/'scripts'/p for p in scripts]
files += [p for p in (root/'src').iterdir() if p.suffix in ('.c','.h','.S') and p.name not in ('embedded_io.c','embedded_io.h')]
for file in files:
    target=out/file.relative_to(root);target.parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(file,target)
manifest={str(p.relative_to(out)).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest()
          for p in out.rglob('*') if p.is_file()}
(out/'SOURCE-MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Staged',len(manifest),'source/metadata files; no build trees or game data')

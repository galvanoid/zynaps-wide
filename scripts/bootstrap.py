"""Fetch pinned source dependencies only. Never downloads game data."""
from pathlib import Path
import shutil,subprocess
root=Path(__file__).resolve().parents[1]
git=shutil.which('git') or r'C:\Program Files\Git\cmd\git.exe'
dependencies=(
    ('atari_reverse','https://github.com/geogeo28/atari_reverse.git','c1b35bfebb9f0466698f7cf211ecb04a22b0a8b1'),
    ('md-devops','https://github.com/sidecartridge/md-devops.git','1b5788d42b63f95ca340830f52da589869531f72'),
)
for name,url,revision in dependencies:
    target=root/'build/deps'/name
    if not target.exists():
        target.parent.mkdir(parents=True,exist_ok=True)
        subprocess.run([git,'clone','--no-checkout',url,str(target)],check=True)
        subprocess.run([git,'-C',str(target),'checkout','--detach',revision],check=True)
    actual=subprocess.check_output([git,'-C',str(target),'rev-parse','HEAD'],text=True).strip()
    if actual!=revision:raise SystemExit(f'Unexpected {name} revision: {actual}; existing checkout left untouched')
    if name=='md-devops':
        subprocess.run([git,'-C',str(target),'submodule','update','--init','--recursive'],check=True)
    print(name,revision)

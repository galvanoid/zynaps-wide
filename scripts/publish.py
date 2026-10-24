"""Publish the verified clean release and submit its origin to the app store.

Dry run by default. Requires gh authentication; never uploads the workspace repo.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import urllib.request

root = Path(__file__).resolve().parents[1]
repo = 'galvanoid/zynaps-wide'
store = 'sidecartridge/sidecartridge-microfirmwares-store'
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--source', type=Path, required=True)
p.add_argument('--execute', action='store_true')
args = p.parse_args()
source = args.source.resolve()
version = (root/'VERSION').read_text().strip()
tag = 'v'+version
status = json.loads((root/'build/DISTRIBUTION-STATUS.json').read_text())
for field in ('local_tests', 'isolated_source_build', 'hardware_installation',
              'physical_gameplay', 'hardware_cache_relaunch'):
    assert status.get(field) == 'passed', 'Still required: '+field
audit = json.loads((root/'build/release-audit.json').read_text())
assert audit['status'] == 'pass'
for name, digest in status['sha256'].items():
    assert hashlib.sha256((root/'dist'/name).read_bytes()).hexdigest() == digest == audit['sha256'][name]
manifest = json.loads((source/'SOURCE-MANIFEST.json').read_text())
for name, digest in manifest.items():
    assert not Path(name).is_absolute() and '..' not in Path(name).parts
    assert hashlib.sha256((source/name).read_bytes()).hexdigest() == digest, name
assert (source/'apps.json').read_bytes() == (root/'apps.json').read_bytes()
assets = [root/'dist'/n for n in ('ZYNAPS-SC.uf2', 'ZYWIDE.PRG', 'SHA256SUMS.txt')]
assets += [root/'build/release'/('Zynaps-Wide-'+version+s+'.zip') for s in ('', '-source')]
assert all(f.is_file() for f in assets)
app = json.loads((root/'apps.json').read_text())['apps'][0]
assert app['md5'] == hashlib.md5(assets[0].read_bytes()).hexdigest()
assert app['binary'] == 'https://github.com/'+repo+'/releases/download/'+tag+'/ZYNAPS-SC.uf2'
print('Ready:', repo, tag, 'with', len(manifest), 'source files and', len(assets), 'release assets', flush=True)
if not args.execute:
    raise SystemExit('Dry run complete; no remote changes.')

gh = shutil.which('gh')
git = shutil.which('git') or r'C:\Program Files\Git\cmd\git.exe'
assert gh, 'Install GitHub CLI first'

def run(argv, cwd=None, check=True):
    result = subprocess.run([str(x) for x in argv], cwd=cwd, text=True,
                            capture_output=True, timeout=120)
    if check and result.returncode:
        raise RuntimeError(result.stderr.strip() or result.stdout.strip())
    return result

def github(*argv, check=True):
    return run([gh, *argv], check=check)

def gitrun(*argv, cwd=source, check=True):
    return run([git, '-c', 'credential.helper=', '-c', 'credential.helper=!gh auth git-credential',
                *argv], cwd=cwd, check=check)

assert json.loads(github('api', 'user').stdout)['login'] == 'galvanoid'
view = github('repo', 'view', repo, '--json', 'nameWithOwner', check=False)
if view.returncode:
    if 'Could not resolve' not in view.stderr and '404' not in view.stderr:
        raise RuntimeError(view.stderr)
    github('repo', 'create', repo, '--public', '--description',
           'Zynaps Wide accelerator for SidecarTridge. Original game files required; none included.')
if not (source/'.git').exists():
    gitrun('init', '-b', 'main')
    gitrun('config', 'user.name', 'galvanoid')
    gitrun('config', 'user.email', '33522344+galvanoid@users.noreply.github.com')
    gitrun('config', 'core.autocrlf', 'false')
    # Explicit allowlist: build/dist fixtures can never enter the source commit.
    gitrun('add', '--', *sorted(manifest), 'SOURCE-MANIFEST.json')
    gitrun('commit', '-m', 'Release asset-free Zynaps Wide '+version)
    gitrun('remote', 'add', 'origin', 'https://github.com/'+repo+'.git')
gitrun('push', '-u', 'origin', 'main')
notes = root/'build/RELEASE-NOTES.md'
assert notes.is_file()
existing = github('release', 'view', tag, '-R', repo, check=False)
if existing.returncode:
    github('release', 'create', tag, *assets, '-R', repo, '--target', 'main',
           '--title', 'Zynaps Wide '+version, '--notes-file', notes)
# Check the public bytes before submitting the catalog entry.
for file in assets[:3]:
    url = 'https://github.com/'+repo+'/releases/download/'+tag+'/'+file.name
    with urllib.request.urlopen(url, timeout=45) as response:
        assert response.read() == file.read_bytes(), 'Public download mismatch: '+file.name
with urllib.request.urlopen('https://raw.githubusercontent.com/'+repo+'/main/apps.json', timeout=30) as response:
    assert json.load(response) == json.loads((root/'apps.json').read_text())
print('Public release and catalog bytes verified.', flush=True)

fork = 'galvanoid/sidecartridge-microfirmwares-store'
if github('repo', 'view', fork, check=False).returncode:
    github('repo', 'fork', store, '--clone=false')
checkout = root/'build/store-submission'
if not checkout.exists():
    run([git, 'clone', 'https://github.com/'+fork+'.git', str(checkout)])
branch = 'add-zynaps-wide'
gitrun('config', 'user.name', 'galvanoid', cwd=checkout)
gitrun('config', 'user.email', '33522344+galvanoid@users.noreply.github.com', cwd=checkout)
gitrun('fetch', 'https://github.com/'+store+'.git', 'main', cwd=checkout)
existing = json.loads(github('pr', 'list', '-R', store, '--head', 'galvanoid:'+branch,
                            '--state', 'open', '--json', 'url').stdout)
if existing:
    print('Store submission:', existing[0]['url'])
    raise SystemExit(0)
gitrun('switch', '-c', branch, 'FETCH_HEAD', cwd=checkout)
registry = checkout/'origins/atari-st.json'
data = json.loads(registry.read_text(encoding='utf-8'))
origin = {'creator': {'id': 'galvanoid', 'name': 'Galvanoid',
                     'url': 'https://github.com/'+repo}, 'official': False,
          'enabled': True, 'url': 'https://raw.githubusercontent.com/'+repo+'/main/apps.json'}
matches = [o for o in data['origins'] if o.get('creator', {}).get('id') == 'galvanoid']
if matches:
    assert matches == [origin], 'Existing creator origin differs; inspect before changing it'
    print('Creator origin is already registered.')
else:
    data['origins'].append(origin)
    registry.write_text(json.dumps(data, indent=2, ensure_ascii=False)+'\n', encoding='utf-8')
    gitrun('add', '--', 'origins/atari-st.json', cwd=checkout)
    gitrun('commit', '-m', 'Register Galvanoid origin for Zynaps Wide', cwd=checkout)
    gitrun('push', '-u', 'origin', branch, cwd=checkout)
    body = root/'build/STORE-PR.md'
    assert body.is_file()
    result = github('pr', 'create', '-R', store, '--base', 'main', '--head', 'galvanoid:'+branch,
                    '--title', 'Add Galvanoid origin: Zynaps Wide', '--body-file', body)
    print(result.stdout.strip())
print('Publication complete. Store listing awaits maintainer review.')

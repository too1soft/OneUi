"""Copy a controlled Go input into an immutable, hashed build-input directory.

This seals provenance, not OS acceptance. Never turn 'sealed' into a supported
platform badge. Both official and patched profiles require native test reports.
"""
import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from compatlib import ContractError, inventory, sha256

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', required=True)
parser.add_argument('--output', required=True)
parser.add_argument('--profile', required=True, choices=['modern', 'legacy'])
args = parser.parse_args()
source, output = Path(args.source).resolve(), Path(args.output).resolve()
if output.exists():
    raise ContractError('toolchain output already exists')
if not (source / 'VERSION').read_text().startswith('go1.25.14\n'):
    raise ContractError('Go version differs from the selected baseline')
if args.profile == 'legacy':
    commit = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip()
    if commit != '08118a942518cc18db507ec182acffe7364a92a3':
        raise ContractError('source revision mismatch')
    # Every source mutation must be in the reviewed patch set. This also
    # catches debug edits left behind during native compatibility diagnosis.
    foundation = Path(__file__).resolve().parents[1]
    from compatlib import load
    recipe = load(foundation / 'go-source.lock.json')
    for item in recipe['patches']:
        if sha256(foundation / item['path']) != item['sha256']:
            raise ContractError('changed patch recipe')
    # Reverse application on a temporary private index must reconstruct HEAD.
    import os, tempfile
    with tempfile.TemporaryDirectory(prefix='compat-seal-') as tmp:
        env = dict(os.environ, GIT_INDEX_FILE=str(Path(tmp) / 'index'))
        base = ['git', '-C', str(source)]
        subprocess.run(base + ['read-tree', 'HEAD'], env=env, check=True)
        subprocess.run(base + ['add', '-u'], env=env, check=True)
        subprocess.run(base + ['add', '-A', '--', 'src'], env=env, check=True)
        for item in reversed(recipe['patches']):
            subprocess.run(base + ['apply', '--cached', '--reverse', str(foundation / item['path'])], env=env, check=True)
        subprocess.run(base + ['diff', '--cached', '--exit-code', 'HEAD'], env=env, check=True)
output.mkdir(parents=True)
for name in ('bin', 'pkg', 'src', 'lib', 'api', 'test', 'misc', 'doc'):
    if (source / name).is_dir():
        shutil.copytree(source / name, output / name)
for path in source.iterdir():
    if path.is_file() and not path.name.startswith('.'):
        shutil.copy2(path, output / path.name)
metadata = {'schemaVersion': 1, 'kind': 'go-toolchain', 'version': '1.25.14', 'profile': args.profile,
            'sourceCommit': '08118a942518cc18db507ec182acffe7364a92a3',
            'acceptance': 'candidate-not-certified', 'files': inventory(output)}
(output / 'toolchain-manifest.json').write_text(json.dumps(metadata, indent=2) + '\n', encoding='utf-8')
print('Sealed ' + str(output))

"""Apply the reviewed patch set only to the exact, isolated Go source revision."""
import argparse
import os
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from compatlib import ContractError, load, sha256


def run(root, *args):
    return subprocess.run(['git', '-C', str(root), *args], check=True, text=True, capture_output=True).stdout.strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', required=True)
    args = parser.parse_args()
    foundation = Path(__file__).resolve().parents[1]
    recipe = load(foundation / 'go-source.lock.json')
    source = Path(args.source).resolve()
    if run(source, 'rev-parse', 'HEAD') != recipe['sourceCommit']:
        raise ContractError('Go source revision mismatch')
    if run(source, 'status', '--porcelain', '--untracked-files=all'):
        raise ContractError('Go source must be pristine; use a fresh isolated checkout')
    paths = []
    for item in recipe['patches']:
        path = foundation / item['path']
        if sha256(path) != item['sha256']:
            raise ContractError('patch hash mismatch: ' + item['path'])
        paths.append(str(path))
    # Check the sequential patch set against a private index first. `git apply
    # --check a b` does not evaluate b against a's resulting source.
    with tempfile.TemporaryDirectory(prefix='compat-index-') as directory:
        environment = dict(os.environ, GIT_INDEX_FILE=str(Path(directory) / 'index'))
        subprocess.run(['git', '-C', str(source), 'read-tree', 'HEAD'], env=environment, check=True)
        for path in paths:
            subprocess.run(['git', '-C', str(source), 'apply', '--cached', path], env=environment, check=True)
    for path in paths:
        run(source, 'apply', path)
    print('Applied reviewed Go compatibility patches; runtime acceptance is still required.')


if __name__ == '__main__':
    try:
        main()
    except (ContractError, subprocess.CalledProcessError) as error:
        print(str(error), getattr(error, 'stderr', ''), file=sys.stderr)
        sys.exit(1)

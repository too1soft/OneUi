"""Run the UUID release verifier on oneui-mac using a clean, exact Git revision."""
import argparse
import base64
import hashlib
import json
from pathlib import Path
import shlex
import subprocess


REMOTE = r'''
import base64, hashlib, json, os, shutil
from pathlib import Path
import subprocess, sys
import urllib.request
p = json.load(sys.stdin)
base = Path.home() / '.cache/oneui-release-review'
work = base / 'uuid-current'
marker = work / '.oneui-uuid-owned-fixture'
if work.is_symlink() or (work.exists() and any(work.iterdir()) and not marker.is_file()):
    raise RuntimeError('Unowned UUID fixture')
work.mkdir(parents=True, exist_ok=True)
marker.write_text('OneUI native UUID release verification\n')
if p.get('cleanup'):
    for phase in ('local', 'public'):
        report = json.loads((work / (phase + '-verification.json')).read_text())
        if not report['passed'] or report['sourceCommit'] != p['commit']:
            raise RuntimeError('Cleanup requires successful matching local and public reports')
    names = ['source', 'registry', 'consumer-local', 'consumer-public']
    names += [prefix + linkage for prefix in ('sdk-build-', 'sdk-prefix-', 'plugin-build-', 'plugin-prefix-', 'independent-plugin-')
              for linkage in ('static', 'shared')]
    names += ['validator.py', 'oneui-site-preview6']
    targets = []
    for name in names:
        path = work / name
        if not path.exists(): continue
        if path.is_symlink() or not path.resolve().is_relative_to(work.resolve()) or path.resolve() == work.resolve():
            raise RuntimeError('Unsafe cleanup target: ' + str(path))
        entries = list(path.rglob('*')) if path.is_dir() else []
        if any(entry.is_symlink() for entry in entries): raise RuntimeError('Linked cleanup descendant')
        opened = subprocess.run(['/usr/sbin/lsof', '-t', '+D', str(path)] if path.is_dir()
                                else ['/usr/sbin/lsof', '-t', str(path)], capture_output=True)
        if opened.returncode not in (0, 1) or opened.stdout.strip(): raise RuntimeError('Cleanup target in use: ' + name)
        targets.append(path)
    removed = []
    for path in targets:
        size = sum(entry.stat().st_size for entry in path.rglob('*') if entry.is_file()) if path.is_dir() else path.stat().st_size
        if path.is_dir(): shutil.rmtree(path)
        else: path.unlink()
        removed.append(dict(path=str(path), logicalBytes=size))
    result = dict(status='complete', removed=removed, retained='Versioned packages, official CLI, reports, compact logs and ownership marker')
    (work / 'cleanup.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))
    raise SystemExit(0)
source = work / 'source'
if source.is_symlink(): raise RuntimeError('Linked source')
if not source.exists(): subprocess.run(['git', 'init', str(source)], check=True)
if subprocess.check_output(['git', '-C', str(source), 'status', '--porcelain'], text=True).strip():
    raise RuntimeError('Unexpected source edits')
subprocess.run(['git', '-C', str(source), 'fetch', '--depth', '1', 'https://github.com/too1soft/OneUi.git', p['commit']], check=True)
subprocess.run(['git', '-C', str(source), 'checkout', '--detach', 'FETCH_HEAD'], check=True)
if subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip() != p['commit']:
    raise RuntimeError('Source mismatch')
cli = work / 'oneui-site'
if p['public']:
    with urllib.request.urlopen('https://oneui.too1.net/api/tools', timeout=30) as response:
        tools = json.load(response)['tools']
    tool = next(t for t in tools if t['platform'] == 'macos' and t['sha256'] == p['cliSha256'])
    with urllib.request.urlopen('https://oneui.too1.net/files/' + tool['file'], timeout=60) as response:
        data = response.read()
    if len(data) != tool['size']: raise RuntimeError('Public CLI size mismatch')
    cli.write_bytes(data)
    print('Downloaded official Mac CLI with pinned SHA256 ' + p['cliSha256'], flush=True)
else:
    cli.write_bytes(base64.b64decode(p['cli']))
if hashlib.sha256(cli.read_bytes()).hexdigest() != p['cliSha256']: raise RuntimeError('CLI mismatch')
cli.chmod(0o755)
validator = work / 'validator.py'
validator.write_bytes(base64.b64decode(p['validator']))
args = [sys.executable, '-B', str(validator), '--source', str(source), '--work', str(work),
        '--output', str(work / 'packages'), '--cli', str(cli)]
if p['public']: args.append('--public')
raise SystemExit(subprocess.run(args).returncode)
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cli', type=Path, required=True)
    parser.add_argument('--source-commit', default='HEAD')
    parser.add_argument('--public', action='store_true')
    parser.add_argument('--cleanup-only', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    output = root / 'out/plugins-current/uuid-macos/mac'
    output.mkdir(parents=True, exist_ok=True)
    commit = subprocess.check_output(['git', '-C', str(root), 'rev-parse', args.source_commit + '^{commit}'], text=True).strip()
    payload = dict(commit=commit, public=args.public, cleanup=args.cleanup_only, cli=base64.b64encode(args.cli.read_bytes()).decode(),
        cliSha256=hashlib.sha256(args.cli.read_bytes()).hexdigest(),
        validator=base64.b64encode((root / 'scripts/test-uuid-release.py').read_bytes()).decode())
    ssh = ['ssh', '-o', 'BatchMode=yes', '-o', 'StrictHostKeyChecking=yes', '-o', 'ConnectTimeout=10', 'oneui-mac']
    process = subprocess.Popen([*ssh, '/usr/bin/python3 -c ' + shlex.quote(REMOTE)], stdin=subprocess.PIPE,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding='utf-8', errors='replace')
    process.stdin.write(json.dumps(payload)); process.stdin.close()
    phase = 'cleanup' if args.cleanup_only else 'public' if args.public else 'local'
    with (output / (phase + '-ssh.log')).open('w', encoding='utf-8') as log:
        for line in process.stdout: print(line, end='', flush=True); log.write(line)
    code = process.wait()
    remote = '.cache/oneui-release-review/uuid-current/'
    if args.cleanup_only:
        if code: raise SystemExit(code)
        (output / 'cleanup.json').write_bytes(subprocess.check_output([*ssh, 'cat ' + remote + 'cleanup.json']))
        return
    for name in (phase + '-verification.json', phase + '.log'):
        result = subprocess.run([*ssh, 'cat ' + shlex.quote(remote + name)], capture_output=True)
        if result.returncode == 0: (output / name).write_bytes(result.stdout)
    if code: raise SystemExit(code)
    if not args.public:
        result = subprocess.check_output([*ssh, 'cat ' + remote + 'packages/plugins.json'])
        catalog = json.loads(result)
        packages = root / 'dist/oneui-plugins/macos/0.1.1'
        packages.mkdir(parents=True, exist_ok=True)
        for package in catalog['packages']:
            for artifact in package['artifacts']:
                data = subprocess.check_output([*ssh, 'cat ' + shlex.quote(remote + 'packages/' + artifact['file'])])
                if len(data) != artifact['size'] or hashlib.sha256(data).hexdigest() != artifact['sha256']:
                    raise RuntimeError('Transferred archive differs')
                path = packages / artifact['file']
                if path.exists() and path.read_bytes() != data: raise RuntimeError('Immutable local archive differs')
                path.write_bytes(data)
        (packages / 'plugins.json').write_bytes(result)


if __name__ == '__main__':
    main()

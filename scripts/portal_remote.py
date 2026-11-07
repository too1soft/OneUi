"""Atomic website, SDK, plugin catalog and CLI deployment for the OneUI mount."""
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import sys
import time
import urllib.request

METADATA = ('releases.json', 'plugins.json', 'tools.json')


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def regular(path):
    if not stat.S_ISREG(path.lstat().st_mode):
        raise ValueError('Expected regular file: ' + path.name)


def command(args):
    p = subprocess.run(args, capture_output=True, text=True, timeout=120)
    if p.returncode:
        raise RuntimeError('Command failed: ' + p.stderr[-1500:])
    return p.stdout


def atomic(path, data, mode=0o644):
    temp = path.with_name(path.name + '.portal-candidate')
    if temp.is_symlink():
        raise ValueError('Linked candidate')
    with temp.open('wb') as out:
        out.write(data)
        out.flush()
        os.fsync(out.fileno())
    os.chmod(temp, mode)
    os.replace(temp, path)


def stamp(root):
    return {name: digest(root / name) if (root / name).exists() else None for name in (*METADATA, 'oneui')}


def catalog(root, name, default):
    p = root / name
    if not p.exists():
        return default
    regular(p)
    return json.loads(p.read_text())


def downloads(root):
    items = []
    for release in catalog(root, 'releases.json', {})['releases']:
        for a in release['artifacts']:
            items.append(a)
            if a.get('runtime'):
                items.append(a['runtime'])
    for release in catalog(root, 'plugins.json', {'packages': []})['packages']:
        items.extend(release['artifacts'])
    items.extend(catalog(root, 'tools.json', {'tools': []})['tools'])
    result = {}
    for item in items:
        name = item['file']
        if not re.fullmatch(r'oneui[.-][A-Za-z0-9._-]+', name) or name in result:
            raise ValueError('Invalid/duplicate published filename')
        result[name] = item
    return result


def restart(config, expected):
    command(['docker', 'restart', '--time', '15', 'oneui'])
    for _ in range(20):
        try:
            base = 'http://127.0.0.1:23187'
            with urllib.request.urlopen(base + '/healthz', timeout=3) as r:
                health = json.load(r)
            matches = health.get('status') == 'ok'
            for name, endpoint in zip(METADATA, ('releases', 'plugins', 'tools')):
                if expected.get(name) is not None:
                    with urllib.request.urlopen(base + '/api/' + endpoint, timeout=3) as r:
                        matches = matches and json.load(r) == expected[name]
            if matches:
                return health
        except OSError:
            pass
        time.sleep(0.5)
    raise RuntimeError('Production did not load the exact staged catalogs')


def clear(stage):
    if stage.name != 'stage' or stage.parent.name != '.oneui-portal' or stage.is_symlink():
        raise ValueError('Invalid stage cleanup boundary')
    for path in stage.iterdir():
        regular(path)
        path.unlink()
    stage.rmdir()


def restore(config, root, state):
    expected = {}
    for name in (*METADATA, 'oneui', 'SHA256SUMS'):
        old = state / ('previous-' + name)
        if old.exists():
            data = old.read_bytes()
            atomic(root / name, data, 0o755 if name == 'oneui' else 0o644)
            if name in METADATA:
                expected[name] = json.loads(data)
        elif name != 'oneui':
            (root / name).unlink(missing_ok=True)
    return restart(config, expected)


def main(request):
    config, action = request['config'], request['action']
    root = Path(config['directory'])
    if root != Path('/apps/oneui') or root.resolve() != root or config['container'] != 'oneui':
        raise ValueError('Unexpected deployment boundary')
    mounts = json.loads(command(['docker', 'inspect', 'oneui', '--format', '{{json .Mounts}}']))
    if not any(m['Type'] == 'bind' and m['Source'] == str(root) and m['Destination'] == '/app' for m in mounts):
        raise ValueError('Verified bind mount changed')
    if action == 'inspect':
        return dict(stamp=stamp(root), catalogs={n: catalog(root, n, None) for n in METADATA}, freeBytes=shutil.disk_usage(root).free)
    state = root / '.oneui-portal'
    if state.is_symlink():
        raise ValueError('Linked state directory')
    state.mkdir(exist_ok=True, mode=0o700)
    locks = root / '.oneui-publish'
    if locks.is_symlink():
        raise ValueError('Linked lock directory')
    locks.mkdir(exist_ok=True, mode=0o700)
    lock_path = locks / 'lock'
    if lock_path.is_symlink():
        raise ValueError('Linked lock')
    with lock_path.open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        stage = state / 'stage'
        if action == 'begin':
            if stamp(root) != request['base'] or not re.fullmatch(r'[0-9a-f-]{36}', request['token']):
                raise ValueError('Production changed; re-plan')
            if shutil.disk_usage(root).free < request['bytes'] + 1024**3:
                raise ValueError('Insufficient deployment space')
            stage.mkdir(mode=0o700)
            atomic(stage / 'transaction.json', json.dumps(request).encode(), 0o600)
            return dict(stage=str(stage))
        if action == 'rollback':
            receipt = json.loads((state / 'receipt.json').read_text())
            if stamp(root) != receipt['after']:
                raise ValueError('Another deployment completed; rollback refused')
            return dict(status='rolled-back', health=restore(config, root, state))
        transaction = json.loads((stage / 'transaction.json').read_text())
        if transaction['token'] != request['token']:
            raise ValueError('Not the staging owner')
        if action == 'abort':
            clear(stage)
            return dict(status='aborted')
        if action != 'commit' or stamp(root) != transaction['base']:
            raise ValueError('Invalid commit or concurrent production change')
        previous = {n: catalog(root, n, None) for n in METADATA}
        expected = {n: catalog(stage, n, None) for n in METADATA}
        for name, field, key in [('releases.json', 'releases', lambda r: r['version']), ('plugins.json', 'packages', lambda r: (r['manifest']['id'], r['manifest']['version']))]:
            incoming = {key(r): r for r in expected[name][field]}
            if any(incoming.get(key(r)) != r for r in (previous[name] or {}).get(field, [])):
                raise ValueError('Published history cannot change')
        files = downloads(stage)
        for name, item in files.items():
            source = stage / name
            if not source.exists():
                regular(root / name)
                os.link(root / name, source)
            regular(source)
            if source.stat().st_size != item['size'] or digest(source) != item['sha256']:
                raise ValueError('Download integrity failure: ' + name)
            if (root / name).exists() and digest(root / name) != item['sha256']:
                raise ValueError('Immutable artifact differs: ' + name)
        binary = stage / transaction['binary']
        if binary.name not in files:
            raise ValueError('Candidate binary must be a verified published CLI')
        os.link(binary, stage / 'oneui')
        os.chmod(stage / 'oneui', 0o755)
        command(['docker', 'exec', 'oneui', '/app/.oneui-portal/stage/oneui', '--data', '/app/.oneui-portal/stage', '--check'])
        for name in (*METADATA, 'oneui', 'SHA256SUMS'):
            backup = state / ('previous-' + name)
            if (root / name).exists():
                regular(root / name)
                atomic(backup, (root / name).read_bytes(), 0o600)
            else:
                backup.unlink(missing_ok=True)
        for name in files:
            if not (root / name).exists():
                os.link(stage / name, root / name)
                os.chmod(root / name, 0o644)
        try:
            for name in METADATA:
                atomic(root / name, (stage / name).read_bytes())
            atomic(root / 'SHA256SUMS', ''.join(a['sha256']+'  '+n+'\n' for n,a in sorted(files.items())).encode())
            atomic(root / 'oneui', binary.read_bytes(), 0o755)
            health = restart(config, expected)
            atomic(state / 'receipt.json', json.dumps(dict(token=request['token'], after=stamp(root))).encode(), 0o600)
        except BaseException:
            restore(config, root, state)
            raise
        finally:
            clear(stage)
        return dict(status='published', health=health, stamp=stamp(root))


if __name__ == '__main__':
    request = json.load(sys.stdin)
    print(json.dumps(main(request)))

"""Remote transaction worker; stdlib only, restricted to the OneUI bind mount."""
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


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def atomic(path, data):
    temp = path.with_name(path.name + '.tmp')
    if temp.is_symlink():
        raise ValueError('Linked temporary file')
    with temp.open('wb') as stream:
        stream.write(data)
        stream.flush()
        os.fsync(stream.fileno())
    os.chmod(temp, 0o644)
    os.replace(temp, path)
    if os.name != 'nt':  # POSIX directory durability; Windows is used only by fixture tests.
        fd = os.open(path.parent, os.O_RDONLY)
        try:
            os.fsync(fd)
        finally:
            os.close(fd)


def regular(path):
    if not stat.S_ISREG(path.lstat().st_mode):
        raise ValueError('Expected regular file: ' + path.name)


def downloads(catalog):
    result = {}
    for release in catalog['releases']:
        for artifact in release['artifacts']:
            for item in [artifact] + ([artifact['runtime']] if artifact.get('runtime') else []):
                name = item['file']
                if not re.fullmatch(r'oneui-[a-zA-Z0-9._-]+\.(zip|dll)', name):
                    raise ValueError('Invalid download filename')
                if name.casefold() in {n.casefold() for n in result}:
                    raise ValueError('Duplicate filename')
                result[name] = item
    return result


def command(args):
    result = subprocess.run(args, capture_output=True, text=True, timeout=120)
    if result.returncode:
        raise RuntimeError('Command failed: ' + result.stderr[-2000:])
    return result.stdout


def check_mount(config, root):
    container = config['container']
    if container != 'oneui' or config['containerDirectory'] != '/app' or config['binary'] != 'oneui':
        raise ValueError('This deployment supports only the verified OneUI container')
    if config['healthUrl'] != 'http://127.0.0.1:23187/healthz':
        raise ValueError('Unexpected health URL')
    mounts = json.loads(command(['docker', 'inspect', container, '--format', '{{json .Mounts}}']))
    if not any(m['Type'] == 'bind' and m['Source'] == str(root) and m['Destination'] == '/app' for m in mounts):
        raise ValueError('OneUI container bind mount changed; inspect before deploying')


def health(config, expected):
    last = None
    for _ in range(20):
        try:
            with urllib.request.urlopen(config['healthUrl'], timeout=3) as response:
                value = json.load(response)
            with urllib.request.urlopen(config['healthUrl'].removesuffix('/healthz') + '/api/releases', timeout=3) as response:
                actual = json.load(response)
            if value.get('status') == 'ok' and actual == expected:
                return value
            last = 'Loaded catalog differs'
        except Exception as error:
            last = type(error).__name__
        time.sleep(0.5)
    raise RuntimeError('OneUI health check failed: ' + str(last))


def restart(config, catalog):
    command(['docker', 'restart', '--time', '15', config['container']])
    return health(config, catalog)


def cleanup(stage):
    # Generated stage is never a download root; no recursive following of links.
    if stage.name != 'stage' or stage.parent.name != '.oneui-publish' or stage.is_symlink():
        raise ValueError('Invalid staging cleanup path')
    for path in stage.iterdir():
        regular(path)
        path.unlink()
    stage.rmdir()


def commit(config, root, state, request):
    stage = state / 'stage'
    transaction = json.loads((stage / 'transaction.json').read_text())
    if transaction['token'] != request['token']:
        raise ValueError('Not the staging owner')
    catalog_path = root / 'releases.json'
    if digest(catalog_path) != transaction['baseHash']:
        raise ValueError('Catalog changed since preparation; refusing lost update')
    old_bytes = catalog_path.read_bytes()
    old = json.loads(old_bytes)
    new_bytes = (stage / 'releases.json').read_bytes()
    if hashlib.sha256(new_bytes).hexdigest() != transaction['newHash']:
        raise ValueError('Staged catalog changed')
    new = json.loads(new_bytes)
    # All history must remain byte-for-byte equivalent as JSON records.
    old_versions = {r['version']: r for r in old['releases']}
    new_versions = {r['version']: r for r in new['releases']}
    if any(new_versions.get(v) != r for v, r in old_versions.items()):
        raise ValueError('Published history cannot change')
    old_files, new_files = downloads(old), downloads(new)
    for name, item in new_files.items():
        source = root / name if name in old_files else stage / name
        regular(source)
        if source.stat().st_size != item['size'] or digest(source) != item['sha256']:
            raise ValueError('Download integrity failure: ' + name)
        if name in old_files:
            os.link(source, stage / name)
        elif (root / name).exists():
            regular(root / name)
            if digest(root / name) != item['sha256']:
                raise ValueError('Immutable file already exists with another hash')
    command(['docker', 'exec', config['container'], '/app/oneui', '--data', '/app/.oneui-publish/stage', '--check'])
    # Verification completed before changing any published metadata.
    for name in new_files.keys() - old_files.keys():
        if not (root / name).exists():
            os.link(stage / name, root / name)
            os.chmod(root / name, 0o644)
    atomic(state / 'previous-releases.json', old_bytes)
    sums = root / 'SHA256SUMS'
    if sums.exists():
        regular(sums)
        atomic(state / 'previous-SHA256SUMS', sums.read_bytes())
    checksums = ''.join(item['sha256'] + '  ' + name + '\n' for name, item in sorted(new_files.items()))
    receipt = dict(baseHash=transaction['baseHash'], newHash=transaction['newHash'], version=request['version'])
    atomic(state / 'receipt.json', json.dumps(receipt).encode())
    try:
        atomic(sums, checksums.encode())
        atomic(catalog_path, new_bytes)
        result = restart(config, new)
    except BaseException:
        atomic(catalog_path, old_bytes)
        if (state / 'previous-SHA256SUMS').exists():
            atomic(sums, (state / 'previous-SHA256SUMS').read_bytes())
        restart(config, old)
        raise
    finally:
        cleanup(stage)
    return dict(status='published', health=result, receipt=receipt)


def rollback(config, root, state, expected_hash=None):
    receipt = json.loads((state / 'receipt.json').read_text())
    if expected_hash and receipt['newHash'] != expected_hash:
        raise ValueError('Another deployment completed; automatic rollback cannot undo it')
    if digest(root / 'releases.json') != receipt['newHash']:
        raise ValueError('Current catalog is not the last deployment; manual review required')
    previous = (state / 'previous-releases.json').read_bytes()
    if hashlib.sha256(previous).hexdigest() != receipt['baseHash']:
        raise ValueError('Rollback catalog corrupted')
    for name, item in downloads(json.loads(previous)).items():
        regular(root / name)
        if digest(root / name) != item['sha256']:
            raise ValueError('Rollback download differs')
    atomic(root / 'releases.json', previous)
    if (state / 'previous-SHA256SUMS').exists():
        atomic(root / 'SHA256SUMS', (state / 'previous-SHA256SUMS').read_bytes())
    return dict(status='rolled-back', health=restart(config, json.loads(previous)))


def main(request):
    config, action = request['config'], request['action']
    raw_root = Path(config['directory'])
    root = raw_root.resolve(strict=True)
    if root != raw_root or root.name != 'oneui' or len(root.parts) < 3:
        raise ValueError('Unexpected OneUI directory or symlink')
    check_mount(config, root)
    regular(root / 'releases.json')
    if action == 'inspect':
        return dict(directory=str(root), catalog=json.loads((root / 'releases.json').read_text()),
                    catalogHash=digest(root / 'releases.json'), freeBytes=shutil.disk_usage(root).free)
    state = root / '.oneui-publish'
    if state.is_symlink():
        raise ValueError('Linked publication state')
    state.mkdir(exist_ok=True, mode=0o700)
    lock_path = state / 'lock'
    if lock_path.is_symlink():
        raise ValueError('Linked publication lock')
    with lock_path.open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        stage = state / 'stage'
        if action == 'begin':
            if digest(root / 'releases.json') != request['baseHash']:
                raise ValueError('Catalog changed; re-plan')
            if shutil.disk_usage(root).free < request['bytes'] + 1024**3:
                raise ValueError('Insufficient server space with 1 GiB reserve')
            stage.mkdir(mode=0o755)  # Existing/interrupted stage requires explicit recovery.
            atomic(stage / 'transaction.json', json.dumps(request).encode())
            return dict(stage=str(stage))
        if action == 'commit':
            return commit(config, root, state, request)
        if action == 'abort':
            if stage.exists():
                if json.loads((stage / 'transaction.json').read_text())['token'] != request['token']:
                    raise ValueError('Not the staging owner')
                cleanup(stage)
            return dict(status='aborted')
        if action == 'rollback':
            return rollback(config, root, state, request.get('expectedHash'))
        raise ValueError('Unknown publication action')


if __name__ == '__main__':
    print(json.dumps(main(json.load(sys.stdin)), ensure_ascii=False))

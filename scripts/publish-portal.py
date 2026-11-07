"""Publish the complete OneUI developer website and matching immutable packages."""
import argparse
import hashlib
import json
from pathlib import Path
import uuid
import urllib.request

from release_payload import prepare, merge
from release_ssh import connect, python
from portal_remote import canonical


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def remote(client, config, action, **fields):
    source = Path(__file__).with_name('portal_remote.py').read_text()
    return json.loads(python(client, source, dict(config=config, action=action, **fields)))


def public(config, catalogs, files):
    class NoRedirect(urllib.request.HTTPRedirectHandler):
        def redirect_request(self, *args, **kwargs):
            raise ValueError('Unexpected public redirect')
    opener = urllib.request.build_opener(NoRedirect)
    base = config['publicUrl'].rstrip('/')
    if base != 'https://oneui.too1.net':
        raise ValueError('Unexpected public origin')
    evidence = []
    for name, endpoint in zip(('releases.json', 'plugins.json', 'tools.json'), ('releases', 'plugins', 'tools')):
        with opener.open(base + '/api/' + endpoint, timeout=30) as response:
            if canonical(json.load(response)) != canonical(catalogs[name]):
                raise ValueError('Public catalog differs: ' + name)
    for route, heading in (('/developers', '开发者中心'), ('/plugins', '插件目录'), ('/docs/plugin-components', '开发和使用组件包')):
        with opener.open(base + route, timeout=30) as response:
            html = response.read().decode()
            if heading not in html:
                raise ValueError('Public page differs: ' + route)
        evidence.append(dict(route=route, status='ok'))
    for name, path in files.items():
        size, hash_value = 0, hashlib.sha256()
        with opener.open(base + '/files/' + name, timeout=60) as response:
            for block in iter(lambda: response.read(1024*1024), b''):
                size += len(block)
                hash_value.update(block)
        if size != path.stat().st_size or hash_value.hexdigest() != digest(path):
            raise ValueError('Public artifact differs: ' + name)
        evidence.append(dict(file=name, size=size, sha256=hash_value.hexdigest()))
    return evidence


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('config', 'sdk', 'plugins', 'tools', 'report'):
        p.add_argument('--' + key, type=Path, required=True)
    p.add_argument('--version', required=True)
    p.add_argument('--published-at', required=True)
    p.add_argument('--apply', action='store_true')
    p.add_argument('--password-stdin', action='store_true')
    args = p.parse_args()
    config = json.loads(args.config.read_text())
    release, files = prepare(args.sdk, args.version, ['windows-modern-x86_64'], args.published_at, args.sdk)
    incoming = json.loads((args.plugins / 'plugins.json').read_text(encoding='utf-8'))
    tools = json.loads((args.tools / 'tools.json').read_text())
    for package in incoming['packages']:
        for artifact in package['artifacts']:
            files[artifact['file']] = args.plugins / artifact['file']
    for tool in tools['tools']:
        files[tool['file']] = args.tools / tool['file']
    binary = next(t['file'] for t in tools['tools'] if t['platform'] == 'linux')
    with connect(config, args.password_stdin) as client:
        before = remote(client, config, 'inspect')
        catalog = merge(before['catalogs']['releases.json'], release)
        plugins = before['catalogs']['plugins.json'] or {'schemaVersion': 1, 'packages': []}
        for package in incoming['packages']:
            existing = next((v for v in plugins['packages'] if v['manifest']['id'] == package['manifest']['id'] and v['manifest']['version'] == package['manifest']['version']), None)
            if existing and existing != package:
                raise ValueError('Published plugin metadata is immutable')
            if not existing:
                plugins['packages'].append(package)
        catalogs = {'releases.json': catalog, 'plugins.json': plugins, 'tools.json': tools}
        plan = dict(version=args.version, sourceCommit=release['sourceCommit'], artifacts=len(files),
                    bytes=sum(f.stat().st_size for f in files.values()), previousVersions=len(before['catalogs']['releases.json']['releases']))
        result = dict(status='plan', plan=plan)
        if args.apply:
            token = str(uuid.uuid4())
            staged = remote(client, config, 'begin', token=token, base=before['stamp'], binary=binary, bytes=plan['bytes'],
                files={n:dict(size=f.stat().st_size,sha256=digest(f)) for n,f in files.items()})
            stage=staged['stage']
            try:
                with client.open_sftp() as sftp:
                    for name, path in files.items():
                        if name in staged['present']:
                            continue
                        print('Uploading ' + name, flush=True)
                        sftp.put(str(path), stage + '/' + name, confirm=True)
                        sftp.chmod(stage + '/' + name, 0o644)
                    for name, data in catalogs.items():
                        with sftp.open(stage + '/' + name, 'wb') as out:
                            out.write((json.dumps(data, ensure_ascii=False, indent=2)+'\n').encode())
                        sftp.chmod(stage + '/' + name, 0o644)
                result = remote(client, config, 'commit', token=token)
            except BaseException:
                try:
                    remote(client, config, 'abort', token=token)
                except Exception:
                    print('Staging cleanup requires inspection', flush=True)
                raise
            try:
                result.update(plan=plan, public=public(config, catalogs, files))
            except BaseException:
                remote(client, config, 'rollback')
                raise
    args.report.write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(result, ensure_ascii=False))


if __name__ == '__main__':
    main()

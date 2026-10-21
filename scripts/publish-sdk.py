"""Publish immutable SDKs to the verified 1Panel OneUI container only."""
import argparse
from datetime import date
import hashlib
import json
from pathlib import Path
import sys
import urllib.request
import uuid

from release_payload import merge, prepare
from release_ssh import connect, python


def remote(client, config, action, **fields):
    source = Path(__file__).with_name('release_remote.py').read_text(encoding='utf-8')
    return json.loads(python(client, source, dict(config=config, action=action, **fields)))


class SameOrigin(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, fp, code, message, headers, url):
        from urllib.parse import urlsplit
        if urlsplit(request.full_url).netloc != urlsplit(url).netloc or urlsplit(url).scheme != 'https':
            raise ValueError('Unexpected public download redirect')
        return super().redirect_request(request, fp, code, message, headers, url)


def verify_public(config, release):
    base = config['publicUrl'].rstrip('/')
    if not base.startswith('https://'):
        raise ValueError('Public verification requires HTTPS')
    opener = urllib.request.build_opener(SameOrigin)
    with opener.open(base + '/api/releases', timeout=30) as stream:
        catalog = json.load(stream)
    if release not in catalog['releases']:
        raise ValueError('Public catalog does not contain the expected release')
    evidence = []
    for artifact in release['artifacts']:
        for item in [artifact] + ([artifact['runtime']] if artifact.get('runtime') else []):
            h, size = hashlib.sha256(), 0
            with opener.open(base + '/files/' + item['file'], timeout=60) as stream:
                for block in iter(lambda: stream.read(1024 * 1024), b''):
                    h.update(block)
                    size += len(block)
            if h.hexdigest() != item['sha256'] or size != item['size']:
                raise ValueError('Public download differs: ' + item['file'])
            evidence.append(dict(file=item['file'], sha256=h.hexdigest(), size=size))
    return evidence


def publish(client, config, release, files, apply=False):
    before = remote(client, config, 'inspect')
    previous = next((r for r in before['catalog']['releases'] if r['version'] == release['version']), None)
    if previous:
        # Re-running on another day must not mutate an immutable publication date.
        release['publishedAt'] = previous['publishedAt']
    merged = merge(before['catalog'], release)
    if merged == before['catalog']:
        return dict(status='already-published', verified=verify_public(config, release))
    data = (json.dumps(merged, ensure_ascii=False, indent=2) + '\n').encode()
    plan = dict(version=release['version'], abi=release['abi'], targetDirectory=config['directory'],
                container=config['container'], files=list(files), bytes=sum(p.stat().st_size for p in files.values()),
                historicalReleases=len(before['catalog']['releases']), sourceCommit=release['sourceCommit'])
    if not apply:
        return dict(status='plan', **plan)
    token = str(uuid.uuid4())
    stage = remote(client, config, 'begin', token=token, baseHash=before['catalogHash'],
                   newHash=hashlib.sha256(data).hexdigest(), bytes=plan['bytes'])['stage']
    try:
        with client.open_sftp() as sftp:
            for name, path in files.items():
                print('Uploading ' + name, flush=True)
                sftp.put(str(path), stage + '/' + name, confirm=True)
                sftp.chmod(stage + '/' + name, 0o644)
            with sftp.open(stage + '/releases.json', 'wb') as stream:
                stream.write(data)
            sftp.chmod(stage + '/releases.json', 0o644)
        committed = remote(client, config, 'commit', token=token, version=release['version'])
    except BaseException:
        # Never retry commit automatically after ambiguous network failures.
        # This removes only our stage. A completed release is left intact.
        try:
            remote(client, config, 'abort', token=token)
        except Exception as error:
            print('Stage cleanup incomplete: ' + str(error), file=sys.stderr)
        raise
    try:
        evidence = verify_public(config, release)
    except Exception:
        remote(client, config, 'rollback', expectedHash=committed['receipt']['newHash'])
        raise
    return dict(status='published', plan=plan, transaction=committed, verified=evidence)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', type=Path, required=True)
    parser.add_argument('--packages', type=Path)
    parser.add_argument('--version')
    parser.add_argument('--targets', nargs='+', default=['windows-modern-x86_64'])
    parser.add_argument('--published-at', default=date.today().isoformat())
    parser.add_argument('--apply', action='store_true', help='Upload and activate; default is a read-only remote plan')
    parser.add_argument('--rollback', action='store_true', help='Restore only the last catalog; keep immutable downloads')
    parser.add_argument('--password-stdin', action='store_true', help='For secret managers; never include a password argument')
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    config = json.loads(args.config.read_text(encoding='utf-8'))
    if args.rollback and not args.apply:
        parser.error('Rollback requires --apply')
    if not args.rollback:
        if not args.packages or not args.version:
            parser.error('--packages and --version are required')
        release, files = prepare(args.packages, args.version, args.targets, args.published_at, args.packages)
    with connect(config, args.password_stdin) as client:
        result = (remote(client, config, 'rollback') if args.rollback else
                  publish(client, config, release, files, args.apply))
    text = json.dumps(result, ensure_ascii=False, indent=2) + '\n'
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(text, encoding='utf-8')
    print(text)


if __name__ == '__main__':
    main()

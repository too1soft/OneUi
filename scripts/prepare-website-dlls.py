"""Publish direct DLL downloads from verified, immutable SDK ZIPs; never build SDKs."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import tempfile
import zipfile


def digest(data):
    return hashlib.sha256(data).hexdigest()


def atomic_write(path, data):
    descriptor, name = tempfile.mkstemp(prefix='.oneui-publish-', dir=path.parent)
    try:
        with os.fdopen(descriptor, 'wb') as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(name, path)
    finally:
        if os.path.exists(name):
            os.unlink(name)


def prepare(directory):
    directory = directory.resolve(strict=True)
    catalog_path = directory / 'releases.json'
    catalog = json.loads(catalog_path.read_text(encoding='utf-8'))
    pending = []
    for release in catalog['releases']:
        for artifact in release['artifacts']:
            if artifact['platform'] != 'windows' or artifact['linkage'] != 'shared':
                continue
            filename = artifact['file']
            if not re.fullmatch(r'[a-zA-Z0-9][a-zA-Z0-9._-]{0,180}\.zip', filename):
                raise ValueError('Invalid SDK filename')
            archive_path = directory / filename
            if archive_path.is_symlink():
                raise ValueError('SDK must not be a link')
            with archive_path.open('rb') as stream:
                archive_hash = hashlib.file_digest(stream, 'sha256').hexdigest()
            if archive_path.stat().st_size != artifact['size'] or archive_hash != artifact['sha256']:
                raise ValueError(f'SDK verification failed: {filename}')
            with zipfile.ZipFile(archive_path) as archive:
                if archive.namelist().count('sdk-manifest.json') != 1 or archive.namelist().count('bin/oneui.dll') != 1:
                    raise ValueError(f'DLL or manifest is missing/ambiguous: {filename}')
                manifest_data = archive.read('sdk-manifest.json')
                if digest(manifest_data) != artifact['manifestSha256']:
                    raise ValueError(f'Manifest verification failed: {filename}')
                entries = [entry for entry in json.loads(manifest_data)['files'] if entry['path'] == 'bin/oneui.dll']
                if len(entries) != 1:
                    raise ValueError('DLL manifest record is missing/ambiguous')
                entry = entries[0]
                if not 0 < entry['size'] <= 256 * 1024 * 1024 or archive.getinfo('bin/oneui.dll').file_size != entry['size']:
                    raise ValueError('Invalid DLL size')
                payload = archive.read('bin/oneui.dll')
            if len(payload) != entry['size'] or digest(payload) != entry['sha256']:
                raise ValueError(f'DLL verification failed: {filename}')
            name = filename.removesuffix('-shared.zip') + '.dll'
            if not name.endswith('.dll') or name == filename:
                raise ValueError('Expected a shared SDK filename')
            runtime = dict(file=name, size=len(payload), sha256=digest(payload))
            if artifact.get('runtime') not in (None, runtime):
                raise ValueError(f'Published runtime metadata cannot be overwritten: {name}')
            target = directory / name
            if target.is_symlink() or (target.exists() and target.read_bytes() != payload):
                raise ValueError(f'Published DLL cannot be overwritten: {name}')
            pending.append((target, payload))
            artifact['runtime'] = runtime
    # Verify all inputs before writing anything, then publish the catalog last.
    for target, payload in pending:
        if not target.exists():
            atomic_write(target, payload)
    atomic_write(catalog_path, (json.dumps(catalog, ensure_ascii=False, indent=2) + '\n').encode('utf-8'))
    print(f'Prepared {len(pending)} verified DLL downloads; SDK archives unchanged.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path, default=Path(__file__).resolve().parents[1] / 'dist/oneui-website')
    prepare(parser.parse_args().directory)

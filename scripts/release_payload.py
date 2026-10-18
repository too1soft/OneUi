"""Validate SDK contents and assemble a release without copying whole archives."""
import hashlib
import importlib.util
import json
from pathlib import Path, PurePosixPath
import re
import zipfile


def module(name):
    spec = importlib.util.spec_from_file_location(name.replace('-', '_'), Path(__file__).with_name(name + '.py'))
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def validate_archive(path, artifact):
    with zipfile.ZipFile(path) as archive:
        members = archive.infolist()
        names = [m.filename for m in members]
        if len(names) != len(set(n.casefold() for n in names)):
            raise ValueError('Duplicate archive member')
        for entry in members:
            rel = PurePosixPath(entry.filename)
            if rel.is_absolute() or '..' in rel.parts or '\\' in entry.filename or ':' in entry.filename:
                raise ValueError('Unsafe archive member')
            if (entry.external_attr >> 16) & 0o170000 == 0o120000:
                raise ValueError('Linked archive member')
        raw = archive.read('sdk-manifest.json')
        if hashlib.sha256(raw).hexdigest() != artifact['manifestSha256']:
            raise ValueError('Wrong manifest hash')
        manifest = json.loads(raw)
        records = manifest['files']
        expected = {e['path'] for e in records} | {'sdk-manifest.json'}
        if len(records) + 1 != len(expected) or set(names) != expected:
            raise ValueError('Manifest does not cover exactly the archive contents')
        if sum(e['size'] for e in records) > 2 * 1024**3:
            raise ValueError('SDK expansion exceeds 2 GiB')
        for entry in records:
            if archive.getinfo(entry['path']).file_size != entry['size']:
                raise ValueError('Member size differs: ' + entry['path'])
            with archive.open(entry['path']) as stream:
                actual = hashlib.file_digest(stream, 'sha256').hexdigest()
            if actual != entry['sha256']:
                raise ValueError('Member hash differs: ' + entry['path'])
        return manifest


def prepare(packages, version, targets, published_at, output):
    packages, output = Path(packages).resolve(strict=True), Path(output).resolve()
    catalog = module('make-release-catalog')
    release = catalog.build_release(packages, packages, version, 'preview', published_at, targets)
    files, provenance = {}, set()
    # Validate everything before creating the standalone DLL.
    for artifact in release['artifacts']:
        path = packages / artifact['file']
        if path.is_symlink():
            raise ValueError('Linked input package')
        manifest = validate_archive(path, artifact)
        provenance.add(manifest.get('sourceTreeSha256', ''))
        files[path.name] = path
    if len(provenance) != 1:
        raise ValueError('Packages have differing source trees')
    if not re.fullmatch('[0-9a-f]{40}', release['sourceCommit']):
        raise ValueError('Missing source revision')
    release['notes'] += [
        'ABI ' + str(release['abi']) + '；头文件、Rust 包与 DLL 必须成套升级，不可混用历史版本。',
        '本次发布仅验证所列产物和下载；尚未完成的原生验收、签名状态以包旁说明为准。',
        '官网内嵌文档仍为 preview.1 快照；本版本接口以下载包中的头文件、绑定和清单为准。']
    output.mkdir(parents=True, exist_ok=True)
    dlls = module('prepare-website-dlls')
    # Reuse the existing extraction/immutability checks on a one-release catalog.
    # No ZIP copies: extract only the validated runtime member here.
    for artifact in release['artifacts']:
        if artifact['platform'] != 'windows' or artifact['linkage'] != 'shared':
            continue
        with zipfile.ZipFile(packages / artifact['file']) as archive:
            payload = archive.read('bin/oneui.dll')
        filename = artifact['file'].removesuffix('-shared.zip') + '.dll'
        target = output / filename
        if target.is_symlink() or (target.exists() and target.read_bytes() != payload):
            raise ValueError('Refusing to replace an existing DLL')
        if not target.exists():
            dlls.atomic_write(target, payload)
        artifact['runtime'] = dict(file=filename, size=len(payload), sha256=dlls.digest(payload))
        files[filename] = target
    return release, files


def merge(catalog, release):
    return module('make-release-catalog').merge_release(catalog, release)

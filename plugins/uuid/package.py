"""Package this independent plugin's installed files; no OneUI publisher changes."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import zipfile


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('build', 'prefix', 'output'):
        p.add_argument('--' + name, type=Path, required=True)
    p.add_argument('--linkage', choices=['static', 'shared'], required=True)
    args = p.parse_args()
    source = Path(__file__).resolve().parent
    recipe = json.loads((args.build / 'uuid-build-Release.json').read_text(encoding='utf-8'))
    windows = recipe['system'] == 'Windows'
    macos = recipe['system'] == 'Darwin'
    if recipe['configuration'] != 'Release' or not (windows or macos) or recipe['pointerBytes'] != 8:
        raise ValueError('This release recipe requires Windows x86_64 or macOS arm64 Release')
    expected = 'SHARED_LIBRARY' if args.linkage == 'shared' else 'STATIC_LIBRARY'
    if recipe['sdkType'] != expected or recipe['compiler'] != ('MSVC' if windows else 'AppleClang'):
        raise ValueError('Plugin linkage/toolchain differs from the consumed SDK')
    if recipe['processor'].lower() not in (('amd64', 'x86_64', 'x64') if windows else ('arm64', 'aarch64')):
        raise ValueError('Plugin architecture differs from the release target')
    if macos:
        cache = (args.build / 'CMakeCache.txt').read_text(encoding='utf-8')
        if not re.search(r'^CMAKE_OSX_DEPLOYMENT_TARGET:[^=]+=13\.0$', cache, re.MULTILINE):
            raise ValueError('Mac release requires an explicit macOS 13.0 deployment target')
        recipe['deploymentTarget'] = '13.0'
    for name, key in [('src/plugin.cpp', 'sourceSha256'), ('include/oneui/plugins/uuid.h', 'headerSha256')]:
        if digest(source / name) != recipe[key]:
            raise ValueError('Plugin source changed after configuration')
    repository = Path(subprocess.check_output(['git', '-C', str(source), 'rev-parse', '--show-toplevel'], text=True).strip())
    relative = source.relative_to(repository).as_posix()
    dirty = subprocess.check_output(['git', '-C', str(repository), 'status', '--porcelain', '--', relative], text=True)
    if dirty.strip():
        raise ValueError('Commit this plugin before packaging; unrelated project changes are not included')
    commit = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip()
    if not re.fullmatch('[0-9a-f]{40}', commit):
        raise ValueError('Missing plugin source identity')
    manifest = json.loads((source / 'oneui-plugin.json').read_text(encoding='utf-8'))
    files = {}
    for path in sorted(args.prefix.rglob('*')):
        if path.is_symlink():
            raise ValueError('No symbolic links in plugin archives')
        if path.is_file():
            files[path.relative_to(args.prefix).as_posix()] = path.read_bytes()
    library = 'lib/oneui_uuid.lib' if windows else 'lib/liboneui_uuid.a'
    if library not in files or 'lib/cmake/OneUIUuid/OneUIUuidConfig.cmake' not in files:
        raise ValueError('Incomplete plugin install')
    files['oneui-plugin.json'] = (json.dumps(manifest, indent=2) + '\n').encode()
    files['LICENSE'] = (source / 'LICENSE').read_bytes()
    recipe['sourceCommit'] = commit
    files['oneui-package-build.json'] = (json.dumps(recipe, indent=2) + '\n').encode()
    inventory = [dict(path=name, size=len(data), sha256=hashlib.sha256(data).hexdigest())
                 for name, data in sorted(files.items())]
    encoded = (json.dumps(dict(schemaVersion=1, files=inventory), indent=2) + '\n').encode()
    files['sdk-manifest.json'] = encoded
    target = 'windows-modern-x86_64' if windows else 'macos-native-arm64'
    name = f"{manifest['id']}-{manifest['version']}-{target}-{args.linkage}.zip"
    args.output.mkdir(parents=True, exist_ok=True)
    destination = args.output / name
    with zipfile.ZipFile(destination.with_suffix('.candidate'), 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        for key, value in sorted(files.items()):
            info = zipfile.ZipInfo(key, date_time=(2026, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, value)
    temporary = destination.with_suffix('.candidate')
    if destination.exists() and digest(destination) != digest(temporary):
        temporary.unlink()
        raise ValueError('A published artifact cannot be replaced')
    temporary.replace(destination)
    artifact = dict(id=target + '-' + args.linkage, platform='windows' if windows else 'macos',
        profile='modern' if windows else 'native', architecture='x86_64' if windows else 'arm64',
        linkage=args.linkage, file=name, size=destination.stat().st_size,
        sha256=digest(destination), manifestSha256=hashlib.sha256(encoded).hexdigest(),
        minimumOS='Windows 10' if windows else 'macOS 13.0',
        toolchain=recipe['compiler'] + ' ' + recipe['compilerVersion'] + (' / MultiThreaded' if windows else ' / libc++'),
        signature='unsigned', validation='Native UUID service and independent C/C++ consumers',
        limitations=[], systemDependencies=['bcrypt'] if windows else ['Security', 'c++'])
    catalog_path = args.output / 'plugins.json'
    catalog = json.loads(catalog_path.read_text(encoding='utf-8')) if catalog_path.exists() else dict(schemaVersion=1, packages=[])
    record = next((item for item in catalog['packages'] if item['manifest']['id'] == manifest['id']), None)
    if record is None:
        record = dict(manifest=manifest, description='通过系统随机数服务生成标准小写 UUIDv4，无需 OneUI 渲染器。',
                      sourceCommit=commit, artifacts=[])
        catalog['packages'].append(record)
    if record['manifest'] != manifest or record['sourceCommit'] != commit:
        raise ValueError('Plugin release identity changed')
    prior = next((item for item in record['artifacts'] if item['id'] == artifact['id']), None)
    if prior and prior != artifact:
        raise ValueError('Plugin artifact metadata is immutable')
    if not prior:
        record['artifacts'].append(artifact)
    catalog_path.write_text(json.dumps(catalog, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(artifact))


if __name__ == '__main__':
    main()

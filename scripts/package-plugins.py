#!/usr/bin/env python3
"""Package installed native SDK/plugin outputs into an immutable plugin catalog."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import zipfile

from sdk_distribution import digest


def inventory(files):
    return [{'path': name, 'size': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
            for name, data in sorted(files.items())]


def selected_files(prefix, patterns):
    result = {}
    for pattern in patterns:
        found = False
        for path in sorted(prefix.glob(pattern)):
            if path.is_symlink():
                raise ValueError('SDK package contains a symbolic link')
            if path.is_file():
                found = True
                result[path.relative_to(prefix).as_posix()] = path.read_bytes()
        if not found:
            raise ValueError('Required installed SDK files missing: ' + pattern)
    return result


def package(source, prefix, recipe, package_id, target, linkage, output):
    folder = {'oneui.native-sdk': 'native-sdk', 'oneui.vpn': 'vpn', 'oneui.status': 'status'}[package_id]
    manifest = json.loads((source / 'plugins' / folder / 'oneui-plugin.json').read_text(encoding='utf-8'))
    windows = recipe['system'] == 'Windows'
    if package_id == 'oneui.native-sdk':
        patterns = ['include/oneui/plugin/*.h', 'include/oneui/vpn/*.h',
                    'lib/cmake/OneUIPlugin/*', 'lib/cmake/OneUIVpn/*']
        libraries = ['oneui_plugin', 'oneui_vpn']
        patterns += ['lib/' + (name + '.lib' if windows else 'lib' + name + '.a')
                     for name in libraries] if linkage == 'static' or windows else []
        if linkage == 'shared':
            extension = '.dll' if windows else '.dylib' if recipe['system'] in ('Darwin', 'iOS') else '.so'
            patterns += [('bin/' + name if windows else 'lib/lib' + name) + extension for name in libraries]
        files = selected_files(prefix, patterns)
        # Native adapters are source assets. They retain the existing platform
        # permissions/lifecycle contract and do not claim a signed app bundle.
        for platform in ('android', 'ios', 'harmonyos'):
            for path in (prefix / 'share/oneui' / platform).rglob('*'):
                if path.is_file() and not path.is_symlink():
                    files[path.relative_to(prefix).as_posix()] = path.read_bytes()
    elif package_id == 'oneui.vpn':
        patterns = ['include/oneui/plugins/vpn.h', 'lib/cmake/OneUIVpnPlugin/*',
                    'lib/oneui_vpn_plugin.lib' if windows else 'lib/liboneui_vpn_plugin.a']
        files = selected_files(prefix, patterns)
    else:
        if recipe.get('coreABI')!=46 or linkage!='shared':
            raise ValueError('Status 0.1 requires shared OneUI ABI 46')
        patterns=['include/oneui/plugins/status.h','lib/cmake/OneUIStatus/*',
                  'share/oneui/plugins/oneui.status/*','lib/oneui_status.lib' if windows else 'lib/liboneui_status.a']
        files=selected_files(prefix,patterns)
    files['oneui-plugin.json'] = (json.dumps(manifest, ensure_ascii=False, indent=2) + '\n').encode()
    files['LICENSE'] = (source / 'LICENSE').read_bytes()
    files['oneui-package-build.json'] = (json.dumps(recipe, sort_keys=True, indent=2) + '\n').encode()
    encoded = (json.dumps({'schemaVersion': 1, 'files': inventory(files)}, sort_keys=True, indent=2) + '\n').encode()
    files['sdk-manifest.json'] = encoded
    filename = f"{package_id}-{manifest['version']}-{target}-{linkage}.zip"
    # ZIP metadata is fixed, so identical inputs produce identical bytes.
    temporary = output / (filename + '.candidate')
    with zipfile.ZipFile(temporary, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, data in sorted(files.items()):
            info = zipfile.ZipInfo(name, date_time=(2026, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, data)
    destination = output / filename
    if destination.exists() and digest(destination) != digest(temporary):
        temporary.unlink()
        raise ValueError('Published plugin version is immutable; increase its version: ' + filename)
    if destination.exists():
        temporary.unlink()
    else:
        temporary.replace(destination)
    platform, profile, architecture = target.split('-', 2)
    artifact = {'id': target + '-' + linkage, 'platform': platform, 'profile': profile,
                'architecture': architecture, 'linkage': linkage, 'file': filename,
                'size': destination.stat().st_size, 'sha256': digest(destination),
                'manifestSha256': hashlib.sha256(encoded).hexdigest(),
                'toolchain': recipe['compiler'] + ' ' + recipe['compilerVersion'] + ' / ' + recipe['runtime'],
                'minimumOS': 'Windows 10' if windows else 'macOS 13.0' if recipe['system'] == 'Darwin' else '', 'signature': 'unsigned',
                'validation': 'native Plugin SDK and independent consumer', 'limitations': [],
                'systemDependencies': []}
    description = {'oneui.native-sdk': '无渲染依赖的插件运行时与原生 VPN SDK。',
                   'oneui.vpn': '通过插件服务接入 OneUI VPN 会话与原生宿主。',
                   'oneui.status': '带进度显示、键盘激活和无障碍语义的状态组件，适用于 OneUI ABI 46。'}[package_id]
    return {'manifest': manifest, 'description': description,
            'sourceCommit': recipe['sourceCommit'], 'artifacts': [artifact]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--prefix', type=Path, required=True)
    parser.add_argument('--target', required=True)
    parser.add_argument('--linkage', choices=['static', 'shared'], required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--package', choices=['native','oneui.native-sdk','oneui.status'],default='native')
    args = parser.parse_args()
    if not re.fullmatch(r'(windows|linux|macos|android|ios|harmonyos)-[a-z0-9_.]+-(x86_64|arm64|x86|armv7)', args.target):
        raise ValueError('Invalid plugin target')
    source = Path(__file__).resolve().parents[1]
    recipe = json.loads((args.build / 'oneui-plugin-build-Release.json').read_text())
    expected_system = {'windows': 'Windows', 'linux': 'Linux', 'macos': 'Darwin',
                       'android': 'Android', 'ios': 'iOS', 'harmonyos': 'OHOS'}[args.target.split('-')[0]]
    if recipe['configuration'] != 'Release' or recipe['system'] != expected_system or recipe['linkage'].lower() != args.linkage:
        raise ValueError('Build recipe differs from the requested release target/linkage')
    if expected_system == 'Darwin':
        cache = (args.build / 'CMakeCache.txt').read_text(encoding='utf-8')
        if not re.search(r'^CMAKE_OSX_DEPLOYMENT_TARGET:[^=]+=13\.0$', cache, re.MULTILINE):
            raise ValueError('Mac release requires an explicit macOS 13.0 deployment target')
        recipe['deploymentTarget'] = '13.0'
    if recipe['sourceDirty'] or not re.fullmatch(r'[0-9a-f]{40}', recipe['sourceCommit']):
        raise ValueError('Plugin publication requires an identified clean build revision')
    commit = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip()
    dirty = subprocess.check_output(['git', '-C', str(source), 'status', '--porcelain'], text=True).strip()
    if dirty or commit != recipe['sourceCommit']:
        raise ValueError('Source revision changed after SDK configuration')
    pointer_bytes = 4 if args.target.endswith(('-x86', '-armv7')) else 8
    if recipe['pointerBytes'] != pointer_bytes:
        raise ValueError('SDK pointer width differs from the requested architecture')
    processor = recipe['processor'].lower()
    if args.target.endswith('-arm64') and processor not in ('arm64', 'aarch64'):
        raise ValueError('The build processor is not ARM64')
    if args.target.endswith('-x86_64') and processor not in ('amd64', 'x86_64', 'x64'):
        raise ValueError('The build processor is not x86_64')
    if expected_system == 'Windows' and not args.target.startswith('windows-modern-'):
        raise ValueError('This native SDK recipe targets modern Windows')
    args.output.mkdir(parents=True, exist_ok=True)
    catalog_path = args.output / 'plugins.json'
    catalog = json.loads(catalog_path.read_text(encoding='utf-8')) if catalog_path.exists() else {'schemaVersion': 1, 'packages': []}
    for package_id in ((args.package,) if args.package != 'native' else ('oneui.native-sdk','oneui.vpn')):
        release = package(source, args.prefix, recipe, package_id, args.target, args.linkage, args.output)
        existing = next((p for p in catalog['packages'] if p['manifest']['id'] == package_id and p['manifest']['version'] == release['manifest']['version']), None)
        if existing:
            if existing['manifest'] != release['manifest'] or existing['sourceCommit'] != release['sourceCommit']:
                raise ValueError('Plugin release metadata is immutable; increase its version')
            incoming = release['artifacts'][0]
            old = next((a for a in existing['artifacts'] if a['id'] == incoming['id']), None)
            if old and old != incoming:
                raise ValueError('Plugin artifact metadata differs from an existing release')
            if not old:
                existing['artifacts'].append(incoming)
                existing['artifacts'].sort(key=lambda a: a['id'])
        else:
            catalog['packages'].append(release)
    catalog['packages'].sort(key=lambda p: (p['manifest']['id'], p['manifest']['version']))
    temporary = catalog_path.with_suffix('.candidate')
    temporary.write_text(json.dumps(catalog, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    temporary.replace(catalog_path)
    print(json.dumps({'catalog': str(catalog_path), 'packages': len(catalog['packages']), 'target': args.target, 'linkage': args.linkage}))


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Build and consume the Plugin SDK without compiling a UI renderer."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess


def run(args, env, log):
    command = [str(x) for x in args]
    log.write(json.dumps(command) + '\n')
    log.flush()
    subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT,
                   check=True, timeout=300)


def remove_fixture(path, root):
    resolved, boundary = path.resolve(), root.resolve()
    if resolved == boundary or not resolved.is_relative_to(boundary):
        raise RuntimeError('Fixture path escaped the fixed QA directory')
    if path.exists():
        if path.is_symlink() or any(p.is_symlink() for p in path.rglob('*')):
            raise RuntimeError('Refusing fixture cleanup with symbolic links')
        shutil.rmtree(path)


def msvc_environment(env):
    if os.name != 'nt':
        raise RuntimeError('MSVC validation requires Windows')
    vswhere = Path(env.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    install = subprocess.check_output([str(vswhere), '-latest', '-products', '*',
        '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
        '-property', 'installationPath'], text=True).strip()
    setup = Path(install) / 'VC/Auxiliary/Build/vcvars64.bat'
    if not setup.is_file():
        raise RuntimeError('MSVC x64 environment is unavailable')
    # Capture the child environment in memory; never write its values to a log.
    output = subprocess.check_output(f'"{setup}" >nul && set', shell=True,
                                     env=env, text=True, errors='replace')
    result = env.copy()
    for line in output.splitlines():
        key, separator, value = line.partition('=')
        if separator and key:
            result[key] = value
    result.update(CC='cl', CXX='cl')
    return result


def validate(root, output, linkage, env):
    build = output / f'build-{linkage}'
    sdk = output / f'qa-sdk-{linkage}'
    relocated = output / f'qa-sdk-relocated-{linkage}'
    source = output / f'qa-source-{linkage}'
    plugin_build = output / f'qa-plugin-build-{linkage}'
    app_build = output / f'qa-app-build-{linkage}'
    for path in (sdk, relocated, source, plugin_build, app_build):
        remove_fixture(path, output)
    output.mkdir(parents=True, exist_ok=True)
    with (output / f'{linkage}.log').open('w', encoding='utf-8') as log:
        run(['cmake', '-S', root, '-B', build, '-G', 'Ninja', '-DONEUI_PLUGIN_ONLY=ON',
             '-DONEUI_BUILD_VPN_PLUGIN=ON', '-DBUILD_TESTING=ON', '-DCMAKE_BUILD_TYPE=Debug',
             f'-DONEUI_PLUGIN_SHARED={"ON" if linkage == "shared" else "OFF"}',
             f'-DONEUI_VPN_SHARED={"ON" if linkage == "shared" else "OFF"}'], env, log)
        run(['cmake', '--build', build, '--parallel', '4'], env, log)
        run(['ctest', '--test-dir', build, '--output-on-failure'], env, log)
        run(['cmake', '--install', build, '--prefix', sdk], env, log)
        sdk.rename(relocated)  # An installed SDK must survive relocation.
        source.mkdir()
        shutil.copytree(root / 'plugins/vpn', source / 'vpn')
        shutil.copytree(root / 'examples/plugins/vpn_app', source / 'app')
        # Only the external plugin's source is compiled against installed SDKs.
        run(['cmake', '-S', source / 'vpn', '-B', plugin_build, '-G', 'Ninja',
             '-DCMAKE_BUILD_TYPE=Debug', f'-DCMAKE_PREFIX_PATH={relocated}'], env, log)
        run(['cmake', '--build', plugin_build, '--parallel', '4'], env, log)
        run(['cmake', '--install', plugin_build, '--prefix', relocated], env, log)
        run(['cmake', '-S', source / 'app', '-B', app_build, '-G', 'Ninja',
             '-DCMAKE_BUILD_TYPE=Debug', f'-DCMAKE_PREFIX_PATH={relocated}'], env, log)
        run(['cmake', '--build', app_build, '--parallel', '4'], env, log)
        consumer_env = env.copy()
        consumer_env['PATH'] = str(relocated / 'bin') + os.pathsep + env.get('PATH', '')
        consumer_env['LD_LIBRARY_PATH'] = str(relocated / 'lib') + os.pathsep + env.get('LD_LIBRARY_PATH', '')
        consumer_env['DYLD_LIBRARY_PATH'] = str(relocated / 'lib') + os.pathsep + env.get('DYLD_LIBRARY_PATH', '')
        if os.name == 'nt' and linkage == 'shared':
            # Prove generated registration staged the DLL dependency closure.
            # The installed SDK is gone before the independent program starts.
            remove_fixture(relocated, output)
            consumer_env = env.copy()
        run([app_build / ('vpn_plugin_consumer.exe' if os.name == 'nt' else 'vpn_plugin_consumer')], consumer_env, log)
    result = {'linkage': linkage, 'lifecycle': 'pass', 'c_consumer': 'pass',
              'external_plugin_build': 'pass', 'relocation': 'pass', 'renderer_required': False,
              'runtime_staging': 'pass' if os.name == 'nt' and linkage == 'shared' else 'not_required_by_this_target'}
    # Retain build caches for the current implementation; remove disposable
    # consumer sources, test copies and install fixtures after successful QA.
    for path in (relocated, source, plugin_build, app_build):
        remove_fixture(path, output)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--linkage', choices=['static', 'shared', 'all'], default='all')
    parser.add_argument('--msvc', action='store_true')
    options = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    output = root / 'out/plugins-current' / ('msvc' if options.msvc else 'host')
    env = msvc_environment(os.environ.copy()) if options.msvc else os.environ.copy()
    output.mkdir(parents=True, exist_ok=True)
    linkages = ['static', 'shared'] if options.linkage == 'all' else [options.linkage]
    results = [validate(root, output, linkage, env) for linkage in linkages]
    (output / 'verification.json').write_text(json.dumps(results, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(results))


if __name__ == '__main__':
    main()

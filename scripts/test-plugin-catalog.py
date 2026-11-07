#!/usr/bin/env python3
"""Exercise real plugin archives through the Go registry/CLI and a C consumer."""
import argparse
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import time
from urllib.request import urlopen

import importlib.util


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--catalog', type=Path, required=True)
    parser.add_argument('--target', default='windows-modern-x86_64')
    parser.add_argument('--msvc', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    spec = importlib.util.spec_from_file_location('plugin_checks', root / 'scripts/test-plugin-sdk.py')
    helper = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helper)
    output = root / 'out/plugins-current'
    fixture = output / 'catalog-consumer'
    helper.remove_fixture(fixture, output)
    fixture.mkdir(parents=True)
    registry = fixture / 'registry'
    registry.mkdir()
    catalog = json.loads(args.catalog.read_text(encoding='utf-8'))
    shutil.copy2(args.catalog, registry / 'plugins.json')
    for release in catalog['packages']:
        for artifact in release['artifacts']:
            shutil.copy2(args.catalog.parent / artifact['file'], registry / artifact['file'])
    (registry / 'releases.json').write_text('{"schemaVersion":1,"releases":[]}\n', encoding='utf-8')
    env = helper.msvc_environment(os.environ.copy()) if args.msvc else os.environ.copy()
    binary = output / ('oneui-site-plugins.exe' if os.name == 'nt' else 'oneui-site-plugins')
    with (output / 'catalog-consumer.log').open('w', encoding='utf-8') as log:
        subprocess.run(['go', 'build', '-trimpath', '-o', str(binary), '.'],
                       cwd=root / 'website/server', env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
        with socket.socket() as available:
            available.bind(('127.0.0.1', 0))
            port = available.getsockname()[1]
        origin = f'http://127.0.0.1:{port}'
        process = subprocess.Popen([str(binary), '--data', str(registry), '--listen', f'127.0.0.1:{port}'],
                                   stdout=log, stderr=subprocess.STDOUT, env=env,
                                   creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
        reports = []
        try:
            deadline = time.monotonic() + 30
            while True:
                if process.poll() is not None:
                    raise RuntimeError('Plugin registry exited; inspect catalog-consumer.log')
                try:
                    with urlopen(origin + '/healthz', timeout=1) as response:
                        if json.load(response)['status'] == 'ok':
                            break
                except OSError:
                    if time.monotonic() > deadline:
                        raise
                    time.sleep(0.1)
            run = lambda values: helper.run(values, env, log)
            for linkage in ('static', 'shared'):
                project = fixture / linkage
                project.mkdir()
                shutil.copy2(root / 'examples/plugins/vpn_app/main.c', project / 'main.c')
                include = f'.oneui/plugins/{args.target}-{linkage}/OneUIProjectPlugins.cmake'
                (project / 'CMakeLists.txt').write_text(
                    'cmake_minimum_required(VERSION 3.16)\nproject(plugin_catalog_consumer LANGUAGES C CXX)\n'
                    f'include("${{CMAKE_CURRENT_LIST_DIR}}/{include}")\n'
                    'add_executable(vpn_plugin_consumer main.c)\n'
                    'set_target_properties(vpn_plugin_consumer PROPERTIES LINKER_LANGUAGE CXX)\n'
                    'oneui_target_plugins(vpn_plugin_consumer REGISTRAR register_app_plugins PLUGINS ${ONEUI_PLUGIN_TARGETS})\n', encoding='utf-8')
                cli = [binary, 'plugin', 'add', '--id', 'oneui.vpn', '--version', '^0.1.0',
                       '--project', project, '--target', args.target, '--linkage', linkage,
                       '--cache', fixture / 'cache', '--url', origin, '--allow-http']
                run(cli)
                lock = json.loads((project / 'oneui.lock.json').read_text())
                packages = lock['plugins'][args.target + '-' + linkage]['packages']
                if set(packages) != {'oneui.vpn', 'oneui.native-sdk'}:
                    raise RuntimeError('Plugin dependency closure is incomplete')
                run(['cmake', '-S', project, '-B', project / 'build', '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release'])
                run(['cmake', '--build', project / 'build', '--parallel', '4'])
                # No SDK directory is available to the Windows runtime loader.
                if os.name == 'nt':
                    (fixture / 'cache').rename(fixture / 'cache-hidden')
                try:
                    run([project / 'build' / ('vpn_plugin_consumer.exe' if os.name == 'nt' else 'vpn_plugin_consumer')])
                finally:
                    if os.name == 'nt':
                        (fixture / 'cache-hidden').rename(fixture / 'cache')
                reports.append({'linkage': linkage, 'registry_download': 'pass', 'dependency_lock': 'pass',
                                'generated_cmake': 'pass', 'native_consumer': 'pass',
                                'runtime_without_sdk': 'pass' if os.name == 'nt' else 'not_applicable'})
        finally:
            process.terminate()
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=5)
        for report in reports:
            project = fixture / report['linkage']
            common = ['--project', project, '--target', args.target, '--linkage', report['linkage'],
                      '--cache', fixture / 'cache', '--allow-http', '--offline']
            run([binary, 'plugin', 'restore', *common])
            run([binary, 'plugin', 'remove', '--id', 'oneui.vpn', *common])
            lock = json.loads((project / 'oneui.lock.json').read_text())
            if lock['plugins'][args.target + '-' + report['linkage']]['packages']:
                raise RuntimeError('Removing the root retained unreachable dependencies')
            report.update(offline_restore='pass', remove='pass')
    (output / 'catalog-verification.json').write_text(json.dumps(reports, indent=2) + '\n', encoding='utf-8')
    helper.remove_fixture(fixture, output)
    print(json.dumps(reports))


if __name__ == '__main__':
    main()

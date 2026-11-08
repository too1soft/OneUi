"""Exercise the standalone UUID plugin using published SDKs and real archives."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import runpy
import shutil
import socket
import subprocess
import time
from urllib.request import urlopen


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--public', action='store_true', help='Verify the production registry after publication')
    args = p.parse_args()
    root = Path(__file__).resolve().parents[1]
    work = root / 'out/plugins-current/uuid'
    work.mkdir(parents=True, exist_ok=True)
    output = root / 'dist/oneui-uuid'
    output.mkdir(parents=True, exist_ok=True)
    cli = root / 'dist/oneui-tools/0.1.0-preview.4/oneui-site-0.1.0-preview.4-windows-amd64.exe'
    helper = runpy.run_path(str(root / 'scripts/test-plugin-sdk.py'))
    env = helper['msvc_environment'](os.environ.copy())
    phase = 'public' if args.public else 'local'
    report = dict(phase=phase, passed=False, platform='windows-modern-x86_64', checks=[])
    with (work / (phase + '.log')).open('w', encoding='utf-8') as log:
        def run(name, values, succeeds=True):
            command = list(map(str, values))
            log.write(json.dumps(command) + '\n'); log.flush()
            result = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=180)
            report['checks'].append(dict(name=name, exit=result.returncode))
            if (result.returncode == 0) != succeeds:
                raise RuntimeError(name + ' failed; inspect ' + str(work / (phase + '.log')))

        with urlopen('https://oneui.too1.net/api/plugins', timeout=30) as response:
            public_catalog = json.load(response)
        native = next(item for item in public_catalog['packages'] if item['manifest']['id'] == 'oneui.native-sdk')
        source = work / 'independent-source'
        helper['remove_fixture'](source, work)
        shutil.copytree(root / 'plugins/uuid', source / 'uuid')
        shutil.copytree(root / 'examples/plugins/uuid_app', source / 'consumer')
        if not args.public:
            for linkage in ('static', 'shared'):
                bootstrap = work / ('bootstrap-' + linkage)
                bootstrap.mkdir(exist_ok=True)
                run('Fetch published ' + linkage + ' SDK', [cli, 'plugin', 'add', '--id', 'oneui.native-sdk',
                    '--version', '0.1.0', '--project', bootstrap, '--target', report['platform'], '--linkage', linkage,
                    '--cache', work / 'cache', '--url', 'https://oneui.too1.net'])
                lock = json.loads((bootstrap / 'oneui.lock.json').read_text())
                sha = lock['plugins'][report['platform'] + '-' + linkage]['packages']['oneui.native-sdk']['artifact']['sha256']
                prefix = work / 'cache' / sha
                build, installed = work / ('external-build-' + linkage), work / ('installed-' + linkage)
                run('Configure independent ' + linkage + ' plugin', ['cmake', '-S', source / 'uuid', '-B', build,
                    '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded',
                    '-DCMAKE_PREFIX_PATH=' + str(prefix)])
                run('Compile plugin only ' + linkage, ['cmake', '--build', build, '--parallel', '4'])
                commands = subprocess.check_output(['ninja', '-C', str(build), '-t', 'commands'], env=env, text=True)
                if any(name in commands for name in ('src/core/', 'src\\core\\', 'skia.lib', 'oneui.dll')):
                    raise RuntimeError('The plugin compiled or linked the UI framework')
                run('Install ' + linkage + ' plugin', ['cmake', '--install', build, '--prefix', installed])
                run('Package ' + linkage + ' plugin', ['python', '-B', root / 'plugins/uuid/package.py',
                    '--build', build, '--prefix', installed, '--linkage', linkage, '--output', output])
        fixture = work / ('consumer-' + phase)
        helper['remove_fixture'](fixture, work)
        fixture.mkdir()
        registry = work / 'registry'
        registry.mkdir(exist_ok=True)
        process = None
        origin = 'https://oneui.too1.net'
        if not args.public:
            catalog = json.loads((output / 'plugins.json').read_text(encoding='utf-8'))
            catalog['packages'].insert(0, native)
            (registry / 'plugins.json').write_text(json.dumps(catalog, ensure_ascii=False), encoding='utf-8')
            (registry / 'releases.json').write_text('{"schemaVersion":1,"releases":[]}')
            for package in catalog['packages']:
                for artifact in package['artifacts']:
                    destination = registry / artifact['file']
                    if package['manifest']['id'] == 'oneui.uuid':
                        shutil.copy2(output / artifact['file'], destination)
                    else:
                        with urlopen(origin + '/files/' + artifact['file'], timeout=30) as response:
                            data = response.read()
                        if len(data) != artifact['size'] or hashlib.sha256(data).hexdigest() != artifact['sha256']:
                            raise RuntimeError('Public SDK archive integrity mismatch')
                        destination.write_bytes(data)
            with socket.socket() as available:
                available.bind(('127.0.0.1', 0)); port = available.getsockname()[1]
            origin = 'http://127.0.0.1:' + str(port)
            process = subprocess.Popen([str(cli), '--data', str(registry), '--listen', '127.0.0.1:' + str(port)],
                                       stdout=log, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
            deadline = time.monotonic() + 30
            while True:
                try:
                    with urlopen(origin + '/healthz', timeout=1) as response:
                        if json.load(response)['status'] == 'ok': break
                except OSError:
                    if time.monotonic() > deadline: raise
                    time.sleep(0.1)
        cases = []
        try:
            for linkage in ('static', 'shared'):
                project = fixture / linkage
                shutil.copytree(source / 'consumer', project)
                target = report['platform'] + '-' + linkage
                common = ['--project', project, '--target', report['platform'], '--linkage', linkage,
                          '--cache', fixture / 'cache', '--url', origin]
                if not args.public: common.append('--allow-http')
                run('Install UUID from ' + phase + ' registry ' + linkage,
                    [cli, 'plugin', 'add', '--id', 'oneui.uuid', '--version', '^0.1.0', *common])
                lock = json.loads((project / 'oneui.lock.json').read_text())
                if set(lock['plugins'][target]['packages']) != {'oneui.uuid', 'oneui.native-sdk'}:
                    raise RuntimeError('Missing dependency closure')
                run('Configure installed ' + linkage, ['cmake', '-S', project, '-B', project / 'build',
                    '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', '-DONEUI_PROJECT_PLUGINS=' +
                    str(project / '.oneui/plugins' / target / 'OneUIProjectPlugins.cmake')])
                run('Build independent consumer ' + linkage, ['cmake', '--build', project / 'build', '--parallel', '4'])
                (fixture / 'cache').rename(fixture / 'cache-hidden')
                try:
                    run('Execute C/C++ without SDK or plugin cache ' + linkage,
                        ['ctest', '--test-dir', project / 'build', '--output-on-failure'])
                finally:
                    (fixture / 'cache-hidden').rename(fixture / 'cache')
                cases.append((project, common, target))
        finally:
            if process:
                process.terminate()
                process.wait(timeout=10)
        for project, common, target in cases:
            run('Offline restore ' + target, [cli, 'plugin', 'restore', *common, '--offline'])
            lock_file = project / 'oneui.lock.json'
            before = lock_file.read_bytes()
            record = json.loads(before)['plugins'][target]['packages']['oneui.uuid']
            header = fixture / 'cache' / record['artifact']['sha256'] / 'include/oneui/plugins/uuid.h'
            original = header.read_bytes()
            try:
                header.write_bytes(original + b'\n// tampered cache\n')
                run('Reject altered archive cache ' + target, [cli, 'plugin', 'restore', *common, '--offline'], succeeds=False)
                if lock_file.read_bytes() != before: raise RuntimeError('Failed restore changed the lock')
            finally:
                header.write_bytes(original)
            run('Restore verified cache ' + target, [cli, 'plugin', 'restore', *common, '--offline'])
            run('Remove plugin ' + target, [cli, 'plugin', 'remove', '--id', 'oneui.uuid', *common, '--offline'])
            if json.loads(lock_file.read_text())['plugins'][target]['packages']:
                raise RuntimeError('Unreachable plugin dependency remained after removal')
        report.update(passed=True, consumerCases=4, nativeSamplesPerConfiguration=4096,
                      frameworkBuildRequired=False, cacheHiddenAtRuntime=True, tamperRejected=True,
                      dependencyLock=True, offlineRestore=True, removal=True)
        (work / (phase + '-verification.json')).write_text(json.dumps(report, indent=2) + '\n')
        helper['remove_fixture'](fixture, work)
        helper['remove_fixture'](source, work)
    print(json.dumps(report))


if __name__ == '__main__':
    main()

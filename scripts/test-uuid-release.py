"""Build immutable UUID/Plugin SDK releases and consume them on the native host."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import runpy
import shutil
import socket
import subprocess
import sys
import time
from urllib.request import urlopen


class Verification:
    def __init__(self, args):
        self.args = args
        self.source, self.work, self.output = args.source.resolve(), args.work.resolve(), args.output.resolve()
        self.cli = args.cli.resolve()
        self.target = 'macos-native-arm64' if sys.platform == 'darwin' else 'windows-modern-x86_64'
        if platform.machine().lower() not in (('arm64',) if sys.platform == 'darwin' else ('amd64', 'x86_64')):
            raise RuntimeError('Run this release verifier on the native target architecture')
        self.env = os.environ.copy()
        if sys.platform == 'win32':
            self.env = runpy.run_path(str(self.source / 'scripts/test-plugin-sdk.py'))['msvc_environment'](self.env)
        elif sys.platform == 'darwin':
            self.env['PATH'] = str(Path.home() / '.cache/oneui-release-review/tools/bin') + ':' + self.env['PATH']
            # Execution must use staged libraries, never a developer override.
            for key in tuple(self.env):
                if key.startswith('DYLD_'): del self.env[key]
        else:
            raise RuntimeError('This verifier targets Windows x64 and macOS arm64')
        marker = self.work / '.oneui-uuid-owned-fixture'
        if self.work.is_symlink() or (self.work.exists() and any(self.work.iterdir()) and not marker.is_file()):
            raise RuntimeError('Refusing an unowned verification directory')
        self.work.mkdir(parents=True, exist_ok=True)
        marker.write_text('OneUI native UUID release verification\n')
        self.output.mkdir(parents=True, exist_ok=True)
        self.phase = 'public' if args.public else 'local'
        self.report_path = self.work / (self.phase + '-verification.json')
        self.report = dict(passed=False, platform=self.target, host=platform.platform(), checks=[],
            sourceCommit=subprocess.check_output(['git', '-C', str(self.source), 'rev-parse', 'HEAD'], text=True).strip(),
            validatorSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            cliSha256=hashlib.sha256(self.cli.read_bytes()).hexdigest(),
            cliVersion=subprocess.check_output([str(self.cli), '--version'], text=True).strip())
        self.log = (self.work / (self.phase + '.log')).open('w', encoding='utf-8')

    def run(self, name, *args, succeeds=True):
        command = list(map(str, args))
        print(name, flush=True)
        self.log.write(json.dumps(command) + '\n'); self.log.flush()
        result = subprocess.run(command, env=self.env, stdout=self.log, stderr=subprocess.STDOUT, timeout=240)
        self.report['checks'].append(dict(name=name, exit=result.returncode, expectedSuccess=succeeds))
        self.log.flush()
        self.save()
        if (result.returncode == 0) != succeeds:
            raise RuntimeError(name + ' failed; inspect ' + str(self.log.name))

    def save(self):
        self.report_path.write_text(json.dumps(self.report, indent=2) + '\n', encoding='utf-8')

    def build(self):
        for linkage in ('static', 'shared'):
            build, prefix = self.work / ('sdk-build-' + linkage), self.work / ('sdk-prefix-' + linkage)
            flags = ['-DCMAKE_OSX_DEPLOYMENT_TARGET=13.0'] if sys.platform == 'darwin' else ['-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded']
            self.run('Configure SDK ' + linkage, 'cmake', '-S', self.source, '-B', build, '-G', 'Ninja',
                '-DCMAKE_BUILD_TYPE=Release', '-DONEUI_PLUGIN_ONLY=ON', '-DONEUI_BUILD_VPN_PLUGIN=ON',
                '-DBUILD_TESTING=ON', '-DONEUI_PLUGIN_SHARED=' + ('ON' if linkage == 'shared' else 'OFF'),
                '-DONEUI_VPN_SHARED=' + ('ON' if linkage == 'shared' else 'OFF'), *flags)
            self.run('Build SDK ' + linkage, 'cmake', '--build', build, '--parallel', '4')
            self.run('SDK lifecycle tests ' + linkage, 'ctest', '--test-dir', build, '--output-on-failure', '--no-tests=error')
            self.run('Install SDK ' + linkage, 'cmake', '--install', build, '--prefix', prefix)
            self.run('Package SDK ' + linkage, sys.executable, '-B', self.source / 'scripts/package-plugins.py',
                '--build', build, '--prefix', prefix, '--target', self.target, '--linkage', linkage,
                '--package', 'oneui.native-sdk', '--output', self.output)
            plugin_source = self.work / ('independent-plugin-' + linkage)
            shutil.copytree(self.source / 'plugins/uuid', plugin_source, dirs_exist_ok=True)
            plugin_build, installed = self.work / ('plugin-build-' + linkage), self.work / ('plugin-prefix-' + linkage)
            self.run('Configure external UUID ' + linkage, 'cmake', '-S', plugin_source, '-B', plugin_build,
                '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_PREFIX_PATH=' + str(prefix), *flags)
            self.run('Compile external UUID ' + linkage, 'cmake', '--build', plugin_build, '--parallel', '4')
            commands = subprocess.check_output(['ninja', '-C', str(plugin_build), '-t', 'commands'], env=self.env, text=True)
            if any(value in commands.lower() for value in ('src/core/', 'src\\core\\', 'skia', 'oneui.dll')):
                raise RuntimeError('External UUID unexpectedly builds or links the renderer')
            self.run('Install UUID ' + linkage, 'cmake', '--install', plugin_build, '--prefix', installed)
            self.run('Package UUID ' + linkage, sys.executable, '-B', self.source / 'plugins/uuid/package.py',
                '--build', plugin_build, '--prefix', installed, '--linkage', linkage, '--output', self.output)

    def registry(self):
        folder = self.work / 'registry'
        folder.mkdir(exist_ok=True)
        catalog = json.loads((self.output / 'plugins.json').read_text(encoding='utf-8'))
        (folder / 'plugins.json').write_text(json.dumps(catalog), encoding='utf-8')
        (folder / 'releases.json').write_text('{"schemaVersion":1,"releases":[]}', encoding='utf-8')
        for package in catalog['packages']:
            for artifact in package['artifacts']:
                shutil.copy2(self.output / artifact['file'], folder / artifact['file'])
        with socket.socket() as available:
            available.bind(('127.0.0.1', 0)); port = available.getsockname()[1]
        origin = 'http://127.0.0.1:' + str(port)
        process = subprocess.Popen([str(self.cli), '--data', str(folder), '--listen', '127.0.0.1:' + str(port)],
            env=self.env, stdout=self.log, stderr=subprocess.STDOUT,
            **({'creationflags': subprocess.CREATE_NO_WINDOW} if sys.platform == 'win32' else {}))
        try:
            deadline = time.monotonic() + 30
            while True:
                try:
                    with urlopen(origin + '/healthz', timeout=1) as response:
                        if json.load(response)['status'] == 'ok': return origin, process
                except OSError:
                    if time.monotonic() >= deadline: raise
                    time.sleep(0.1)
        except BaseException:
            process.terminate(); process.wait(timeout=10)
            raise

    def consume(self, origin):
        fixture = self.work / ('consumer-' + self.phase)
        fixture.mkdir(exist_ok=True)
        for linkage in ('static', 'shared'):
            project = fixture / linkage
            shutil.copytree(self.source / 'examples/plugins/uuid_app', project, dirs_exist_ok=True)
            target = self.target + '-' + linkage
            common = ['--project', project, '--target', self.target, '--linkage', linkage,
                '--cache', fixture / 'cache', '--url', origin]
            if not self.args.public: common.append('--allow-http')
            self.run('Install UUID ' + linkage, self.cli, 'plugin', 'add', '--id', 'oneui.uuid',
                '--version', '0.1.1', *common)
            lock_file = project / 'oneui.lock.json'
            packages = json.loads(lock_file.read_text())['plugins'][target]['packages']
            if set(packages) != {'oneui.uuid', 'oneui.native-sdk'} or any(p['manifest']['version'] != '0.1.1' for p in packages.values()):
                raise RuntimeError('Missing versioned dependency closure')
            self.run('Configure consumer ' + linkage, 'cmake', '-S', project, '-B', project / 'build', '-G', 'Ninja',
                '-DCMAKE_BUILD_TYPE=Release', '-DONEUI_PROJECT_PLUGINS=' +
                str(project / '.oneui/plugins' / target / 'OneUIProjectPlugins.cmake'))
            self.run('Build consumer ' + linkage, 'cmake', '--build', project / 'build', '--parallel', '4')
            (fixture / 'cache').rename(fixture / 'cache-hidden')
            try:
                self.run('Run C/C++ with all caches hidden ' + linkage, 'ctest', '--test-dir', project / 'build',
                    '--output-on-failure', '--no-tests=error')
                if sys.platform == 'darwin':
                    binary = project / 'build/uuid_plugin_consumer'
                    self.run('Check native Mach-O dependencies ' + linkage, 'otool', '-L', binary)
                    self.run('Verify native executable signature ' + linkage, 'codesign', '--verify', '--strict', binary)
                    if linkage == 'shared' and not (binary.parent / 'liboneui_plugin.dylib').is_file():
                        raise RuntimeError('Mac runtime library was not staged')
            finally:
                (fixture / 'cache-hidden').rename(fixture / 'cache')
            self.run('Offline restore ' + linkage, self.cli, 'plugin', 'restore', *common, '--offline')
            before = lock_file.read_bytes()
            header = fixture / 'cache' / packages['oneui.uuid']['artifact']['sha256'] / 'include/oneui/plugins/uuid.h'
            original = header.read_bytes()
            try:
                header.write_bytes(original + b'\n// altered cache\n')
                self.run('Reject tampered cache ' + linkage, self.cli, 'plugin', 'restore', *common, '--offline', succeeds=False)
                if before != lock_file.read_bytes(): raise RuntimeError('Rejected restore changed the lock')
            finally:
                header.write_bytes(original)
            self.run('Restore verified cache ' + linkage, self.cli, 'plugin', 'restore', *common, '--offline')
            self.run('Remove UUID ' + linkage, self.cli, 'plugin', 'remove', '--id', 'oneui.uuid', *common, '--offline')
            if json.loads(lock_file.read_text())['plugins'][target]['packages']:
                raise RuntimeError('Removal left an unreachable dependency')
        self.report.update(passed=True, consumerCases=4, nativeSamplesPerConfiguration=4096,
            cacheHiddenAtRuntime=True, tamperRejected=True, dependencyLock=True, offlineRestore=True, removal=True,
            uuidRendererBuildRequired=False)
        self.save()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[1])
    for name in ('cli', 'work', 'output'): parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--public', action='store_true')
    verification = Verification(parser.parse_args())
    server = None
    try:
        origin = 'https://oneui.too1.net'
        if not verification.args.public:
            verification.build()
            origin, server = verification.registry()
        verification.consume(origin)
    finally:
        if server: server.terminate(); server.wait(timeout=10)
        verification.save()
        verification.log.close()
    print(json.dumps(verification.report))


if __name__ == '__main__':
    main()

"""Native Mac verification worker; invoked by verify-plugins-macos.py over SSH."""
import datetime
import base64
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import time
import zipfile


class Verification:
    def __init__(self, payload):
        self.payload = payload
        home = Path.home()
        self.base = home / '.cache/oneui-release-review'
        self.root = self.base / 'plugins-current'
        if self.root.is_symlink() or self.root.resolve().parent != self.base.resolve():
            raise RuntimeError('Unexpected verification directory')
        marker = self.root / '.oneui-owned-fixture'
        if marker.is_symlink():
            raise RuntimeError('Unexpected verification ownership marker')
        if self.root.exists() and not marker.is_file():
            raise RuntimeError('Refusing to reuse an unowned verification directory')
        self.root.mkdir(parents=True, exist_ok=True)
        marker.write_text('OneUI macOS plugin verification fixture\n')
        self.source = self.root / 'source'
        self.env = os.environ.copy()
        self.env['PATH'] = str(self.base / 'tools/bin') + ':' + str(home / '.cargo/bin') + ':/usr/bin:/bin:/usr/sbin:/sbin'
        self.env.update(CARGO_TARGET_DIR=str(self.root / 'rust'), CARGO_INCREMENTAL='0')
        self.report = dict(passed=False, sourceCommit=payload['commit'], platform=platform.platform(),
                           architecture=platform.machine(), checks=[],
                           testedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                           scope='Native arm64 service/component SDK contracts, external consumers and host-owned ad-hoc Bundle; not release signing, notarization, x86_64 or VPN traffic')

    def save(self):
        (self.root / 'verification.json').write_text(json.dumps(self.report, indent=2) + '\n')

    def run(self, name, *args, cwd=None, timeout=600, env=None):
        number = len(self.report['checks']) + 1
        log = self.root / ('%02d-%s.log' % (number, re.sub('[^a-z0-9]+', '-', name.lower()).strip('-')))
        command = list(map(str, args))
        print(name, flush=True)
        started = time.monotonic()
        with log.open('w') as stream:
            stream.write(json.dumps(command) + '\n')
            stream.flush()
            result = subprocess.run(command, cwd=cwd, env=env or self.env, stdout=stream,
                                    stderr=subprocess.STDOUT, timeout=timeout)
        text = log.read_text(errors='replace')
        self.report['checks'].append(dict(name=name, exit=result.returncode,
            seconds=round(time.monotonic() - started, 2), log=str(log),
            logSha256=hashlib.sha256(log.read_bytes()).hexdigest(), output=text[-5000:]))
        self.save()
        if result.returncode:
            raise RuntimeError(name + ': ' + text[-5000:])
        return text

    def prepare(self):
        if platform.system() != 'Darwin' or platform.machine() != 'arm64':
            raise RuntimeError('This verification requires the native arm64 Mac')
        if shutil.disk_usage(self.root).free < 12 * 1024**3:
            raise RuntimeError('Insufficient disk headroom')
        if self.source.is_symlink() or self.source.resolve().parent != self.root.resolve():
            raise RuntimeError('Unexpected verification source directory')
        for name in ('cmake', 'ninja', 'cargo', 'clang++'):
            if not shutil.which(name, path=self.env['PATH']):
                raise RuntimeError('Missing existing build tool: ' + name)
        if self.source.exists():
            status = subprocess.check_output(['git', '-C', str(self.source), 'status', '--porcelain'], text=True)
            if any(line[3:] != 'scripts/test-component-sdk.py' for line in status.splitlines()):
                raise RuntimeError('Verification source has unexpected local edits: ' + status)
            self.run('Restore owned validation overlay', 'git', '-C', self.source,
                     'checkout', '--', 'scripts/test-component-sdk.py')
        else:
            self.run('Initialize isolated source', 'git', 'init', self.source)
        self.run('Fetch exact source commit', 'git', '-C', self.source, 'fetch', '--depth', '1',
                 'https://github.com/too1soft/OneUi.git', self.payload['commit'])
        self.run('Select exact source commit', 'git', '-C', self.source, 'checkout', '--detach', 'FETCH_HEAD')
        if subprocess.check_output(['git', '-C', str(self.source), 'rev-parse', 'HEAD'], text=True).strip() != self.payload['commit']:
            raise RuntimeError('Source revision mismatch')
        validator = self.payload['componentValidator'].encode()
        self.report['validationOverlay'] = dict(path='scripts/test-component-sdk.py',
                                               sha256=hashlib.sha256(validator).hexdigest())
        self.report['compiler'] = self.run('Apple compiler identity', 'clang++', '--version').splitlines()[1:]
        self.report['skiaRevision'] = subprocess.check_output(['git', '-C',
            str(Path.home() / 'projects/oneui/third_party/skia'), 'rev-parse', 'HEAD'], text=True).strip()

    def native(self):
        self.run('Service SDK static and shared consumers', 'python3', '-B',
                 self.source / 'scripts/test-plugin-sdk.py', cwd=self.source)
        self.report['service'] = json.loads((self.source / 'out/plugins-current/host/verification.json').read_text())
        build = self.root / 'ui'
        self.run('Configure native UI SDK', 'cmake', '-S', self.source, '-B', build, '-G', 'Ninja',
            '-DCMAKE_BUILD_TYPE=Release', '-DONEUI_BUILD_STATIC=ON', '-DBUILD_TESTING=ON',
            '-DONEUI_ENABLE_GPU=ON', '-DONEUI_ENABLE_YOGA=ON',
            '-DONEUI_ENABLE_DEVELOPER_TOOLS=OFF', '-DONEUI_ENABLE_TEST_FRAME_CAPTURE=OFF',
            '-DONEUI_BUNDLED_SKIA_ROOT=' + str(Path.home() / 'projects/oneui/third_party/skia'),
            '-DONEUI_BUNDLED_SKIA_OUT=' + str(self.base / 'skia'),
            '-DONEUI_TEXT_TEST_ASSET_ROOT=' + str(self.base / 'text-assets'),
            '-DFETCHCONTENT_SOURCE_DIR_ONEUI_YOGA=' + str(self.base / 'build/_deps/oneui_yoga-src'))
        tests = ['oneui_component_tests', 'oneui_view_cpp_diff_tests', 'oneui_view_runtime_tests', 'oneui_view_reload_tests',
                 'oneui_control_behavior_tests', 'oneui_view_c_api_tests',
                 'oneui_macos_backend_tests', 'oneui_metal_rendering_tests',
                 'oneui_overlay_host_behavior_tests', 'oneui_overlay_host_lifetime_tests',
                 'oneui_desktop_renderer_tests']
        self.run('Build native SDK and regression targets', 'cmake', '--build', build, '--parallel',
                 str(self.payload['jobs']), '--target', 'oneui', 'oneui_static', 'oneui-viewc', *tests, timeout=1800)
        pattern = '^(' + '|'.join(tests[:-1] + ['oneui_desktop_renderer_software',
            'oneui_desktop_renderer_gpu', 'oneui_abi_sync', 'oneui_macos_configuration_tests']) + ')$'
        self.run('Native regression and Cocoa Metal acceptance', 'ctest', '--test-dir', build,
                 '--output-on-failure', '-R', pattern, '--no-tests=error', timeout=180)
        self.run('Independent component and relocated Bundle consumer', 'python3', '-B',
                 *self.component_validator(),
                 '--build', build, cwd=self.source)
        self.report['component'] = json.loads((self.source / 'out/plugins-current/component-verification.json').read_text())
        env = self.env.copy()
        env.update(ONEUI_LIB_DIR=str(build), DYLD_LIBRARY_PATH=str(build))
        self.run('Native Rust ABI layout and function signatures', 'cargo', 'run', '--locked',
                 '--manifest-path', self.source / 'bindings/rust/Cargo.toml', '-p', 'oneui-abi-tests',
                 env=env, timeout=900)

    def component_validator(self):
        script = self.source / 'scripts/test-component-sdk.py'
        script.write_text(self.payload['componentValidator'])
        return [script]

    def packaged(self):
        """Exercise real Mac SDK archives and the native CLI against local HTTP."""
        self.run('Restore validator before clean package configuration', 'git', '-C', self.source,
                 'checkout', '--', 'scripts/test-component-sdk.py')
        licenses = self.root / 'licenses'
        skia = Path.home() / 'projects/oneui/third_party/skia'
        notices = ['harfbuzz/COPYING', 'icu/LICENSE', 'expat/COPYING',
                   'libjpeg-turbo/LICENSE.md', 'libjpeg-turbo/README.ijg', 'libpng/LICENSE',
                   'libwebp/COPYING', 'libwebp/PATENTS', 'libwebp/AUTHORS', 'zlib/LICENSE',
                   'freetype/LICENSE.TXT', 'freetype/docs/FTL.TXT', 'freetype/docs/GPLv2.TXT']
        for relative in notices:
            destination = licenses / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(skia / 'third_party/externals' / relative, destination)
        (licenses / 'skia').mkdir(exist_ok=True)
        shutil.copy2(skia / 'LICENSE', licenses / 'skia/LICENSE')
        shutil.copy2(self.root / 'ui/licenses/Yoga-LICENSE.txt', licenses / 'Yoga-LICENSE.txt')
        packages = self.root / 'sdk-packages'
        artifact_file = packages / 'artifacts-macos-native-arm64.json'
        if not artifact_file.exists():
            self.run('Package static and shared Mac SDK archives', 'python3', '-B',
                self.source / 'scripts/package-versioned-sdk.py', '--source', self.source,
                '--build', self.root / 'ui', '--output', packages, '--licenses', licenses,
                '--platform', 'macos', '--architecture', 'arm64', '--profile', 'native',
                '--toolchain', 'AppleClang-17-libc++', '--minimum-os', 'macOS 13.0',
                '--version', '0.1.0-preview.4', '--source-commit', self.payload['commit'])
        self.report['sdkArtifacts'] = json.loads(artifact_file.read_text())
        for artifact in self.report['sdkArtifacts']:
            package = packages / artifact['file']
            if hashlib.sha256(package.read_bytes()).hexdigest() != artifact['sha256']:
                raise RuntimeError('SDK package changed after its manifest was created')
        self.run('Packaged C and C++ consumers static and shared', 'python3', '-B',
            self.source / 'scripts/test-versioned-sdk.py', '--packages', packages,
            '--work', self.root / 'sdk-consumer-current', '--target', 'macos-native-arm64',
            '--version', '0.1.0-preview.4', '--report', self.root / 'sdk-consumers.json')
        self.run('Packaged safe Rust consumers static and shared', 'python3', '-B',
            self.source / 'scripts/test-versioned-rust-sdk.py', '--packages', packages,
            '--work', self.root / 'rust-sdk-consumer-current', '--cargo-target', self.root / 'rust-sdk',
            '--target', 'macos-native-arm64', '--version', '0.1.0-preview.4',
            '--report', self.root / 'rust-sdk-consumers.json', timeout=900)
        self.report['sdkConsumers'] = json.loads((self.root / 'sdk-consumers.json').read_text())
        self.report['rustConsumers'] = json.loads((self.root / 'rust-sdk-consumers.json').read_text())
        # Python's ZIP extractor omits Unix execute bits; restore only the safe
        # executable/file modes the CLI extractor also preserves.
        extracted = self.root / 'shared-sdk'
        if not extracted.exists():
            shared = next(a for a in self.report['sdkArtifacts'] if a['linkage'] == 'shared')
            with zipfile.ZipFile(packages / shared['file']) as archive:
                for entry in archive.infolist():
                    relative = Path(entry.filename)
                    if relative.is_absolute() or '..' in relative.parts or '\\' in entry.filename:
                        raise RuntimeError('Unsafe SDK package entry')
                    archive.extract(entry, extracted)
                    path = extracted / relative
                    if path.is_file():
                        path.chmod(0o755 if entry.external_attr >> 16 & 0o111 else 0o644)
        self.run('Component consumer of immutable SDK archive', 'python3', '-B',
                 *self.component_validator(), '--sdk', extracted, cwd=self.source)
        self.run('Restore validators for native plugin packaging', 'git', '-C', self.source,
                 'checkout', '--', 'scripts/test-component-sdk.py')
        if self.payload.get('cli'):
            self.catalog()

    def catalog(self):
        cli = self.root / 'oneui-site'
        cli.write_bytes(base64.b64decode(self.payload['cli']))
        cli.chmod(0o755)
        self.report['cliSha256'] = hashlib.sha256(cli.read_bytes()).hexdigest()
        self.run('Execute Darwin arm64 CLI', cli, '--version')
        catalog = self.root / 'plugin-packages'
        for linkage in ('static', 'shared'):
            build, prefix = self.root / ('native-' + linkage), self.root / ('native-sdk-' + linkage)
            flag = 'ON' if linkage == 'shared' else 'OFF'
            self.run('Configure Release native ' + linkage, 'cmake', '-S', self.source, '-B', build,
                '-G', 'Ninja', '-DONEUI_PLUGIN_ONLY=ON', '-DONEUI_BUILD_VPN_PLUGIN=ON',
                '-DBUILD_TESTING=OFF', '-DCMAKE_BUILD_TYPE=Release', '-DONEUI_PLUGIN_SHARED=' + flag,
                '-DONEUI_VPN_SHARED=' + flag)
            self.run('Build Release native ' + linkage, 'cmake', '--build', build, '--parallel', str(self.payload['jobs']))
            self.run('Install Release native ' + linkage, 'cmake', '--install', build, '--prefix', prefix)
            self.run('Package native ' + linkage, 'python3', '-B', self.source / 'scripts/package-plugins.py',
                '--build', build, '--prefix', prefix, '--target', 'macos-native-arm64',
                '--linkage', linkage, '--output', catalog)
        validator = self.source / 'scripts/test-plugin-catalog.py'
        validator.write_text(self.payload['catalogValidator'])
        self.report['catalogValidatorSha256'] = hashlib.sha256(validator.read_bytes()).hexdigest()
        try:
            self.run('Mac native HTTP install lock CMake offline restore and remove', 'python3', '-B',
                validator, '--catalog', catalog / 'plugins.json', '--target', 'macos-native-arm64', '--cli', cli)
            self.report['catalog'] = json.loads((self.source / 'out/plugins-current/catalog-verification.json').read_text())
        finally:
            self.run('Restore owned catalog validator', 'git', '-C', self.source,
                     'checkout', '--', 'scripts/test-plugin-catalog.py')

    def execute(self):
        exit_code = 0
        try:
            if self.payload.get('cleanupOnly'):
                self.report = json.loads((self.root / 'verification.json').read_text())
                if not self.report.get('passed') or self.report['sourceCommit'] != self.payload['commit']:
                    raise RuntimeError('Cleanup requires a successful report of the selected source commit')
                self.cleanup()
            else:
                self.prepare()
                self.native()
                self.packaged()
                self.report['sourceStatus'] = subprocess.check_output(
                    ['git', '-C', str(self.source), 'status', '--porcelain'], text=True).strip()
                self.report['passed'] = True
        except Exception as error:
            exit_code = 1
            if self.payload.get('cleanupOnly'):
                self.report.setdefault('cleanup', {})['status'] = 'blocked'
                self.report['cleanup']['reason'] = str(error)
            else:
                self.report['error'] = str(error)
            print(str(error), flush=True)
        finally:
            self.report['finishedAt'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
            self.save()
        print(json.dumps(dict(passed=self.report['passed'], checks=len(self.report['checks']),
                             report=str(self.root / 'verification.json'))), flush=True)
        return exit_code if exit_code else (0 if self.report['passed'] else 1)

    def cleanup(self):
        processes = subprocess.check_output(['ps', '-axo', 'pid=,args='], text=True, errors='replace')
        users = [line for line in processes.splitlines() if str(self.root) in line]
        if users:
            raise RuntimeError('Verification outputs remain in use: ' + '\n'.join(users))
        if self.source.exists():
            status = subprocess.check_output(['git', '-C', str(self.source), 'status', '--porcelain'], text=True)
            if status.strip():
                raise RuntimeError('Verification source acquired local edits; retained: ' + status)
            for name in ('static', 'shared'):
                shutil.copy2(self.source / ('out/plugins-current/host/' + name + '.log'),
                             self.root / ('service-' + name + '.log'))
            catalog_log = self.source / 'out/plugins-current/catalog-consumer.log'
            if catalog_log.exists():
                shutil.copy2(catalog_log, self.root / 'catalog-consumer.log')
        candidates = []
        for name in ('source', 'ui', 'rust', 'rust-sdk', 'native-static', 'native-shared',
                     'native-sdk-static', 'native-sdk-shared', 'shared-sdk', 'licenses'):
            path = self.root / name
            if not path.exists():
                continue
            if path.resolve().parent != self.root.resolve() or path.is_symlink():
                raise RuntimeError('Unsafe cleanup path: ' + str(path))
            files = list(path.rglob('*'))
            if any(p.is_symlink() for p in files):
                raise RuntimeError('Cleanup directory contains a symlink; retained: ' + str(path))
            size = sum(p.stat().st_size for p in files if p.is_file())
            candidates.append((path, size))
        self.report['cleanup'] = dict(status='in_progress', removed=[],
            retained='SDK and plugin archives, CLI, verification reports, compact logs and the ownership marker')
        self.save()
        for path, size in candidates:
            shutil.rmtree(path)
            self.report['cleanup']['removed'].append(dict(path=str(path), logicalBytes=size))
            self.save()
        self.report['cleanup']['status'] = 'complete'


if __name__ == '__main__':
    raise SystemExit(Verification(json.load(sys.stdin)).execute())

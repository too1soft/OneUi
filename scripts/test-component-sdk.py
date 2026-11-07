"""Compile an independent component and .one consumer against prebuilt OneUI."""
import argparse
import json
import os
from pathlib import Path
import platform
import plistlib
import re
import runpy
import shutil
import subprocess


def macos_bundle(work, application, runtime, run):
    """The host owns Bundle assembly; the registrar only stages Windows DLLs."""
    bundle = work / 'StatusConsumer.app'
    contents = bundle / 'Contents'
    binary = contents / 'MacOS/status_app'
    library = contents / 'Frameworks/liboneui.dylib'
    binary.parent.mkdir(parents=True)
    library.parent.mkdir()
    shutil.copy2(application, binary)
    shutil.copy2(runtime, library)
    (contents / 'Info.plist').write_bytes(plistlib.dumps(dict(
        CFBundleIdentifier='org.oneui.status-consumer', CFBundleExecutable='status_app',
        CFBundleName='OneUI Status Consumer', CFBundlePackageType='APPL',
        CFBundleVersion='1', CFBundleShortVersionString='0.1.0')))
    run('install_name_tool', '-id', '@rpath/liboneui.dylib', library)
    for path, rpath in ((binary, '@executable_path/../Frameworks'), (library, '@loader_path')):
        dependencies = [line.strip().split(' (')[0]
                        for line in run('otool', '-L', path).splitlines()[1:]]
        for dependency in dependencies:
            if dependency.endswith('/liboneui.dylib') and dependency != '@rpath/liboneui.dylib':
                run('install_name_tool', '-change', dependency, '@rpath/liboneui.dylib', path)
        old_paths = re.findall(r'cmd LC_RPATH\s+cmdsize \d+\s+path (.*?) \(offset',
                               run('otool', '-l', path))
        for old in old_paths:
            run('install_name_tool', '-delete_rpath', old, path)
        run('install_name_tool', '-add_rpath', rpath, path)
        run('codesign', '--force', '--sign', '-', path)
    run('codesign', '--force', '--sign', '-', '--identifier', 'org.oneui.status-consumer', bundle)
    run('codesign', '--verify', '--deep', '--strict', bundle)
    # Reject every non-system dependency that is not supplied by this Bundle.
    for path in (binary, library):
        for line in run('otool', '-L', path).splitlines()[1:]:
            dependency = line.strip().split(' (')[0]
            if dependency.startswith(('/System/Library/', '/usr/lib/')):
                continue
            if not dependency.startswith('@rpath/') or not (library.parent / dependency[7:]).is_file():
                raise RuntimeError('Runtime dependency escapes the Bundle: ' + dependency)
    return binary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=Path('out/plugins-current/ui'))
    parser.add_argument('--sdk', type=Path, help='Validate the extracted immutable SDK package')
    parser.add_argument('--plugin', type=Path, help='Validate an extracted prebuilt component package')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    work = root / 'out/plugins-current/component-sdk'
    # Only this script's regenerable fixture belongs to this cleanup boundary.
    if work.exists():
        assert work.resolve().is_relative_to(root / 'out/plugins-current')
        shutil.rmtree(work)
    work.mkdir()
    build = args.build.resolve()
    env = os.environ.copy()
    if os.name == 'nt':
        env = runpy.run_path(str(root / 'scripts/test-plugin-sdk.py'))['msvc_environment'](env)
    system = platform.system()
    if system not in ('Windows', 'Darwin', 'Linux'):
        raise RuntimeError('Unsupported native consumer platform: ' + system)

    def run(*cmd):
        result = subprocess.run(list(map(str, cmd)), env=env, capture_output=True, text=True, errors='replace')
        if result.returncode:
            raise RuntimeError(result.stdout[-6000:] + result.stderr[-2000:])
        return result.stdout

    sdk = work / 'sdk'
    if args.sdk:
        shutil.copytree(args.sdk, sdk)
    else:
        shutil.copytree(root / 'include', sdk / 'include')
        (sdk / 'cmake').mkdir()
        for name in ('OneUIConfig.cmake', 'OneUITargets.cmake', 'OneUIComponents.cmake', 'OneUIPluginRuntimes.cmake', 'OneUIView.cmake'):
            shutil.copy2(root / 'cmake' / name, sdk / 'cmake' / name)
        (sdk / 'lib').mkdir()
        (sdk / 'bin').mkdir()
        if os.name == 'nt':
            shutil.copy2(build / 'oneui.lib', sdk / 'lib/oneui.lib')
            shutil.copy2(build / 'oneui.dll', sdk / 'bin/oneui.dll')
        else:
            name = 'liboneui.dylib' if system == 'Darwin' else 'liboneui.so'
            shutil.copy2(build / name, sdk / 'lib' / name)
        name = 'oneui-viewc.exe' if os.name == 'nt' else 'oneui-viewc'
        shutil.copy2(build / name, sdk / 'bin' / name)
    source = work / 'plugin-source'
    shutil.copytree(root / 'plugins/status', source)
    plugin_build, prefix = work / 'plugin-build', work / 'installed-plugin'
    if args.plugin:
        shutil.copytree(args.plugin,prefix)
    else:
        run('cmake', '-S', source, '-B', plugin_build, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
            '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded', f'-DOneUI_DIR={sdk / "cmake"}', f'-DCMAKE_INSTALL_PREFIX={prefix}')
        run('cmake', '--build', plugin_build, '--parallel', '4')
        run('cmake', '--install', plugin_build)
    app_source, app_build = work / 'app-source', work / 'app-build'
    shutil.copytree(root / 'examples/plugins/status_app', app_source)
    run('cmake', '-S', app_source, '-B', app_build, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
        f'-DOneUI_DIR={sdk / "cmake"}', f'-DCMAKE_PREFIX_PATH={prefix}')
    run('cmake', '--build', app_build, '--parallel', '4')
    application = app_build / ('status_app.exe' if os.name == 'nt' else 'status_app')
    if system == 'Darwin':
        application = macos_bundle(work, application, sdk / 'lib/liboneui.dylib', run)
    elif system == 'Linux':
        runtime = work / 'runtime'
        runtime.mkdir()
        shutil.copy2(sdk / 'lib/liboneui.so', runtime / 'liboneui.so')
        env['LD_LIBRARY_PATH'] = str(runtime)
    # A missing SDK proves runtime copying follows the linked package closure.
    sdk.rename(work / 'sdk-hidden')
    try:
        output = run(application)
    finally:
        (work / 'sdk-hidden').rename(sdk)
    report = dict(independentSource=True, prebuiltSDK=True, noFrameworkBuild=True,
                  installedPlugin=True, compilerContracts=True, propertyBinding=True,
                  keyboardEvent=True, accessibility=True, lateCallbackClosed=True, runtimeStaging=True,
                  platform=system, architecture=platform.machine(),
                  hostBundle=system == 'Darwin', releaseSigningVerified=False)
    (root / 'out/plugins-current/component-verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(output.strip())
    shutil.rmtree(work)


if __name__ == '__main__':
    main()

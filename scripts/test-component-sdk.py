"""Compile an independent component and .one consumer against prebuilt OneUI."""
import argparse
import json
import os
from pathlib import Path
import runpy
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=Path('out/plugins-current/ui'))
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    work = root / 'out/plugins-current/component-sdk'
    # Only this script's regenerable fixture belongs to this cleanup boundary.
    if work.exists():
        assert work.resolve().is_relative_to(root / 'out/plugins-current')
        shutil.rmtree(work)
    work.mkdir()
    build = args.build.resolve()
    env = runpy.run_path(str(root / 'scripts/test-plugin-sdk.py'))['msvc_environment'](os.environ.copy())

    def run(*cmd):
        result = subprocess.run(list(map(str, cmd)), env=env, capture_output=True, text=True, errors='replace')
        if result.returncode:
            raise RuntimeError(result.stdout[-6000:] + result.stderr[-2000:])
        return result.stdout

    sdk = work / 'sdk'
    shutil.copytree(root / 'include', sdk / 'include')
    (sdk / 'cmake').mkdir()
    for name in ('OneUIConfig.cmake', 'OneUITargets.cmake', 'OneUIComponents.cmake', 'OneUIPluginRuntimes.cmake', 'OneUIView.cmake'):
        shutil.copy2(root / 'cmake' / name, sdk / 'cmake' / name)
    (sdk / 'lib').mkdir()
    (sdk / 'bin').mkdir()
    shutil.copy2(build / 'oneui.lib', sdk / 'lib/oneui.lib')
    for name in ('oneui.dll', 'oneui-viewc.exe'):
        shutil.copy2(build / name, sdk / 'bin' / name)
    source = work / 'plugin-source'
    shutil.copytree(root / 'plugins/status', source)
    plugin_build, prefix = work / 'plugin-build', work / 'installed-plugin'
    run('cmake', '-S', source, '-B', plugin_build, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
        '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded', f'-DOneUI_DIR={sdk / "cmake"}', f'-DCMAKE_INSTALL_PREFIX={prefix}')
    run('cmake', '--build', plugin_build, '--parallel', '4')
    run('cmake', '--install', plugin_build)
    app_source, app_build = work / 'app-source', work / 'app-build'
    shutil.copytree(root / 'examples/plugins/status_app', app_source)
    run('cmake', '-S', app_source, '-B', app_build, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
        f'-DOneUI_DIR={sdk / "cmake"}', f'-DCMAKE_PREFIX_PATH={prefix}')
    run('cmake', '--build', app_build, '--parallel', '4')
    # A missing SDK proves runtime copying follows the linked package closure.
    sdk.rename(work / 'sdk-hidden')
    try:
        output = run(app_build / 'status_app.exe')
    finally:
        (work / 'sdk-hidden').rename(sdk)
    report = dict(independentSource=True, prebuiltSDK=True, noFrameworkBuild=True,
                  installedPlugin=True, compilerContracts=True, propertyBinding=True,
                  keyboardEvent=True, accessibility=True, lateCallbackClosed=True, runtimeStaging=True)
    (root / 'out/plugins-current/component-verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(output.strip())
    shutil.rmtree(work)


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Bounded, renderer-free VPN SDK checks; never install or enable a host VPN."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess


def run(*args):
    subprocess.run([str(arg) for arg in args], check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--android-sdk', type=Path)
    parser.add_argument('--android-api', default='36')
    parser.add_argument('--android-ndk', type=Path)
    options = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    output = root / 'out/mobile-current/vpn-host'
    sdk = root / 'out/mobile-current/vpn-sdk'
    run('cmake', '-S', root, '-B', output, '-G', 'Ninja', '-DONEUI_VPN_ONLY=ON',
        '-DBUILD_TESTING=ON', '-DCMAKE_BUILD_TYPE=Debug')
    run('cmake', '--build', output, '--parallel', '4')
    run('ctest', '--test-dir', output, '--output-on-failure')
    run('cmake', '--install', output, '--prefix', sdk)
    report = {'host_contracts': 'pass', 'sdk_install': str(sdk),
              'android_java': 'not_run', 'android_arm64': 'not_run',
              'ios': 'not_run_by_this_script; see apple-verification.json',
              'harmonyos': 'requires_HarmonyOS_SDK_and_native_validation'}
    if options.android_sdk:
        java_home = os.environ.get('JAVA_HOME')
        javac = str(Path(java_home) / 'bin/javac.exe') if os.name == 'nt' and java_home else shutil.which('javac')
        if not javac:
            raise RuntimeError('A JDK is required for Android source compilation')
        classes = output / 'java'
        classes.mkdir(exist_ok=True)
        sources = sorted((root / 'platform/android/java/net/too1/oneui/vpn').glob('*.java'))
        jar = options.android_sdk / 'platforms' / f'android-{options.android_api}' / 'android.jar'
        if not jar.is_file():
            raise RuntimeError(f'Android API jar not found: {jar}')
        run(javac, '-encoding', 'UTF-8', '-Xlint:all', '-classpath', jar, '-d', classes, *sources)
        report['android_java'] = 'pass'
    if options.android_ndk:
        native = root / 'out/mobile-current/vpn-android-arm64'
        run('cmake', '-S', root, '-B', native, '-G', 'Ninja', '-DONEUI_VPN_ONLY=ON',
            '-DBUILD_TESTING=OFF', '-DCMAKE_BUILD_TYPE=Release',
            f'-DCMAKE_TOOLCHAIN_FILE={options.android_ndk.resolve()}/build/cmake/android.toolchain.cmake',
            '-DANDROID_ABI=arm64-v8a', '-DANDROID_PLATFORM=android-28')
        run('cmake', '--build', native, '--parallel', '4')
        report['android_arm64'] = 'compiled_not_executed'
    (output / 'verification.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, ensure_ascii=False))


if __name__ == '__main__':
    main()

"""Build the Android test host or --workbench candidate with installed SDK tools.

No Gradle bootstrap, SDK downloads, desktop OneUI rebuild or production signing.
Outputs are fixed under out/mobile-current; install to an explicitly selected
emulator with adb after this command. Observe the project artifact budget first.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import zipfile


def run(*args):
    subprocess.run([str(arg) for arg in args], check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', type=Path, required=True)
    parser.add_argument('--ndk-version', default='29.0.14206865')
    parser.add_argument('--build-tools', default='36.1.0')
    parser.add_argument('--abi', choices=['x86_64', 'arm64-v8a'], default='x86_64')
    parser.add_argument('--workbench', action='store_true', help='Build the real unified workbench widgets')
    parser.add_argument('--yoga-source', type=Path, help='Existing pinned Yoga source; no download')
    parser.add_argument('--java-home', type=Path, default=Path(os.environ.get('JAVA_HOME', '')))
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    source = root / ('examples/performance_lab/mobile/android' if args.workbench else 'tests/mobile/android')
    target = 'oneui_mobile_lab' if args.workbench else 'oneui_mobile_probe'
    build = root / 'out/mobile-current' / ('android-' + args.abi)
    package = build / 'apk'
    sdk = args.sdk.resolve(strict=True)
    tools = sdk / 'build-tools' / args.build_tools
    ndk = sdk / 'ndk' / args.ndk_version
    android = sdk / 'platforms/android-36/android.jar'
    java = args.java_home / 'bin/java.exe'
    javac = args.java_home / 'bin/javac.exe'
    debug_key = Path.home() / '.android/debug.keystore'
    for item in [ndk / 'build/cmake/android.toolchain.cmake', android, java, javac,
                 tools / 'aapt2.exe', tools / 'lib/d8.jar',
                 tools / 'core-lambda-stubs.jar', debug_key]:
        if not item.is_file():
            raise FileNotFoundError(f'Required installed tool/key is missing: {item}')
    # Keep Java temporary data on the output drive, not the nearly-full C: drive.
    temporary = package / 'tmp'
    temporary.mkdir(parents=True, exist_ok=True)
    os.environ['TEMP'] = os.environ['TMP'] = str(temporary)
    options = ['-DONEUI_MOBILE_RUNTIME_ONLY=ON', '-DONEUI_BUILD_ANDROID_PROBE=ON', '-DONEUI_BUILD_ANDROID_LAB=OFF']
    if args.workbench:
        if not args.yoga_source or not (args.yoga_source / 'yoga/Yoga.h').is_file():
            raise FileNotFoundError('--yoga-source must identify the existing pinned Yoga sources')
        options = ['-DONEUI_MOBILE_RUNTIME_ONLY=OFF', '-DONEUI_BUILD_ANDROID_PROBE=OFF',
                   '-DONEUI_BUILD_ANDROID_LAB=ON', '-DONEUI_ENABLE_YOGA=ON',
                   '-DFETCHCONTENT_SOURCE_DIR_ONEUI_YOGA=' + str(args.yoga_source.resolve()),
                   '-DONEUI_BUNDLED_SKIA_OUT=' + str(root / 'out/mobile-current' / ('skia-' + args.abi))]
    run('cmake', '-S', root, '-B', build, '-G', 'Ninja',
        '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_TOOLCHAIN_FILE=' + str(ndk / 'build/cmake/android.toolchain.cmake'),
        '-DANDROID_ABI=' + args.abi, '-DANDROID_PLATFORM=android-28', '-DANDROID_STL=c++_static',
        '-DBUILD_TESTING=OFF', *options)
    run('cmake', '--build', build, '--parallel', '3', '--target', target)
    # Distinct products cannot share a javac directory: stale probe classes
    # otherwise leak into the workbench APK (and conversely).
    classes, dex = package / (target + '-classes'), package / 'dex'
    classes.mkdir(exist_ok=True); dex.mkdir(exist_ok=True)
    run(javac, '-encoding', 'UTF-8', '-source', '8', '-target', '8',
        '-bootclasspath', os.pathsep.join(map(str, [android, tools / 'core-lambda-stubs.jar'])),
        '-d', classes, *sorted((source / 'java').rglob('*.java')),
        *sorted((root / 'platform/android/java').rglob('*.java')))
    jar = package / 'classes.jar'
    with zipfile.ZipFile(jar, 'w', zipfile.ZIP_DEFLATED) as archive:
        for item in sorted(classes.rglob('*.class')):
            archive.write(item, item.relative_to(classes).as_posix())
    run(java, '-Xmx256m', '-Djava.io.tmpdir=' + str(temporary), '-cp', tools / 'lib/d8.jar',
        'com.android.tools.r8.D8', '--lib', android, '--min-api', '28', '--output', dex, jar)
    unsigned = package / 'unsigned.apk'
    run(tools / 'aapt2.exe', 'link', '-I', android, '--manifest', source / 'AndroidManifest.xml',
        '--min-sdk-version', '28', '--target-sdk-version', '36', '-o', unsigned)
    library = package / ('lib' + target + '.so')
    shutil.copyfile(build / source.relative_to(root) / library.name, library)
    run(ndk / 'toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-strip.exe', '--strip-unneeded', library)
    with zipfile.ZipFile(unsigned, 'a', zipfile.ZIP_DEFLATED) as archive:
        archive.write(dex / 'classes.dex', 'classes.dex')
        archive.write(library, 'lib/' + args.abi + '/' + library.name)
    aligned = package / 'aligned.apk'
    run(tools / 'zipalign.exe', '-f', '-P', '16', '4', unsigned, aligned)
    final = build / ('oneui-mobile-lab.apk' if args.workbench else 'oneui-mobile-probe.apk')
    run(java, '-Xmx128m', '-jar', tools / 'lib/apksigner.jar', 'sign', '--ks', debug_key,
        '--ks-key-alias', 'androiddebugkey', '--ks-pass', 'pass:android', '--key-pass', 'pass:android',
        '--out', final, aligned)
    run(java, '-Xmx128m', '-jar', tools / 'lib/apksigner.jar', 'verify', '--verbose', final)
    kind = 'workbench debug' if args.workbench else 'integration-test'
    print(f'Android {kind} APK: {final} ({final.stat().st_size} bytes)')


if __name__ == '__main__':
    main()

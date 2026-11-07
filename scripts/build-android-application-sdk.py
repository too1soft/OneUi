#!/usr/bin/env python3
"""Compile OneUI once, then assemble a fixed prebuilt SDK for app consumers.

Uses the installed NDK and already built, pinned Skia. Does not install apps or
download dependencies. Audit the project-wide artifact budget before invoking.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ndk', type=Path, required=True)
    parser.add_argument('--sdk', type=Path, required=True)
    parser.add_argument('--abi', choices=['x86_64', 'arm64-v8a'], default='x86_64')
    parser.add_argument('--jobs', type=int, default=4, choices=range(1, 9))
    options = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    build = root / 'out/mobile-current' / ('android-' + options.abi)
    sdk = root / 'out/mobile-current/widget-sdk' / options.abi
    skia = root / 'out/mobile-current' / ('skia-' + options.abi)
    build.mkdir(parents=True, exist_ok=True); sdk.mkdir(parents=True, exist_ok=True)
    if shutil.disk_usage(build).free < 22 * 1024**3: raise RuntimeError('Insufficient disk headroom')
    if not (skia / 'libskia.a').is_file(): raise RuntimeError('Build the matching pinned Android Skia first')
    checks = []

    def run(*command, data=None):
        subprocess.run([str(a) for a in command], input=data, text=True, check=True)
        checks.append(str(command[0]))

    run('cmake', '-S', root, '-B', build, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', '-DBUILD_TESTING=OFF',
        '-DONEUI_BUILD_ANDROID_APPLICATION_HOST=ON', '-DONEUI_BUILD_ANDROID_LAB=OFF',
        '-DONEUI_BUILD_VPN=ON', '-DONEUI_VPN_ANDROID_NATIVE=ON', '-DONEUI_BUILD_JSON=ON', '-DONEUI_ENABLE_YOGA=OFF',
        '-DONEUI_BUNDLED_SKIA_OUT=' + str(skia),
        '-DCMAKE_TOOLCHAIN_FILE=' + str(options.ndk / 'build/cmake/android.toolchain.cmake'),
        '-DANDROID_ABI=' + options.abi, '-DANDROID_PLATFORM=android-28', '-DANDROID_STL=c++_shared')
    run('cmake', '--build', build, '--parallel', options.jobs, '--target', 'oneui_android_application_host', 'oneui_vpn_native', 'oneui_native_services', 'oneui_vpn', 'oneui_json')
    lib = sdk / 'lib'; lib.mkdir(exist_ok=True)
    closure = ['skia', 'skparagraph', 'skshaper', 'skunicode_icu', 'skunicode_core', 'harfbuzz', 'icu',
               'expat', 'jpeg', 'jpeg12', 'jpeg16', 'png', 'webp', 'skcms', 'zlib', 'freetype2']
    if options.abi == 'x86_64': closure.append('webp_sse41')
    archives = [build / (f'lib{name}.a') for name in ['oneui_android_application_host', 'oneui_android_widgets', 'oneui_mobile_runtime', 'oneui_text', 'oneui_json']]
    archives += [skia / ('lib' + name + '.a') for name in closure]
    for archive in archives:
        if not archive.is_file(): raise RuntimeError('Missing reviewed SDK dependency: ' + str(archive))
    archive = lib / 'liboneui_android_application.a'
    ar = options.ndk / 'toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-ar.exe'
    # MRI adds archive members directly; no temporary dependency/extraction copies.
    run(ar, '-M', data='CREATE ' + str(archive) + '\n' + ''.join('ADDLIB ' + str(p) + '\n' for p in archives) + 'SAVE\nEND\n')
    run(ar.parent / 'llvm-strip.exe', '--strip-debug', archive)
    for name in ['liboneui_vpn_native.so', 'liboneui_native_services.so', 'liboneui_vpn.a']:
        shutil.copy2(build / name, lib / name)
    architecture = 'x86_64-linux-android' if options.abi == 'x86_64' else 'aarch64-linux-android'
    shutil.copy2(options.ndk / ('toolchains/llvm/prebuilt/windows-x86_64/sysroot/usr/lib/' + architecture + '/libc++_shared.so'), lib / 'libc++_shared.so')
    shutil.copytree(root / 'include', sdk / 'include', dirs_exist_ok=True)
    shutil.copy2(root / 'bindings/go/oneuivpn/native/native_bridge.h', sdk / 'include/oneui/vpn/native_bridge.h')
    licenses = sdk / 'licenses'; licenses.mkdir(exist_ok=True)
    notices = {'oneui.txt': 'LICENSE', 'yyjson.txt': 'third_party/yyjson/LICENSE', 'skia.txt': 'third_party/skia/LICENSE',
        'skcms-source-notice.txt': 'third_party/skia/modules/skcms/README.chromium',
        'harfbuzz.txt': 'third_party/skia/third_party/externals/harfbuzz/COPYING',
        'icu.txt': 'third_party/skia/third_party/externals/icu/LICENSE',
        'expat.txt': 'third_party/skia/third_party/externals/expat/expat/COPYING',
        'freetype.txt': 'third_party/skia/third_party/externals/freetype/docs/FTL.TXT',
        'jpeg.txt': 'third_party/skia/third_party/externals/libjpeg-turbo/LICENSE.md',
        'jpeg-ijg.txt': 'third_party/skia/third_party/externals/libjpeg-turbo/README.ijg',
        'png.txt': 'third_party/skia/third_party/externals/libpng/LICENSE',
        'webp.txt': 'third_party/skia/third_party/externals/libwebp/COPYING',
        'zlib.txt': 'third_party/skia/third_party/externals/zlib/LICENSE'}
    for name, source in notices.items(): shutil.copy2(root / source, licenses / name)
    classes = build / 'java-sdk'; classes.mkdir(exist_ok=True)
    jar = options.sdk / 'platforms/android-36/android.jar'
    jdk = Path(os.environ['JAVA_HOME']) / 'bin'
    sources = sorted((root / 'platform/android/java').rglob('*.java'))
    run(jdk / 'javac.exe', '-encoding', 'UTF-8', '-source', '8', '-target', '8',
        '-bootclasspath', str(jar) + ';' + str(options.sdk / 'build-tools/36.1.0/core-lambda-stubs.jar'), '-d', classes, *sources)
    with zipfile.ZipFile(sdk / 'oneui-android.jar', 'w', zipfile.ZIP_DEFLATED) as output:
        for path in classes.rglob('*.class'): output.write(path, path.relative_to(classes).as_posix())
    cmake = sdk / 'lib/cmake/OneUIAndroid'; cmake.mkdir(parents=True, exist_ok=True)
    (cmake / 'OneUIAndroidConfig.cmake').write_text('''get_filename_component(_oneui_sdk "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
add_library(OneUI::android_application STATIC IMPORTED)
set_target_properties(OneUI::android_application PROPERTIES
 IMPORTED_LOCATION "${_oneui_sdk}/lib/liboneui_android_application.a"
 INTERFACE_INCLUDE_DIRECTORIES "${_oneui_sdk}/include"
 INTERFACE_COMPILE_FEATURES cxx_std_17
 INTERFACE_COMPILE_DEFINITIONS "ONEUI_STATIC;ONEUI_MOBILE_STATIC"
 INTERFACE_LINK_LIBRARIES "EGL;GLESv3;android;log;dl"
 INTERFACE_LINK_OPTIONS "-Wl,-u,oneui_android_application_link_anchor;-Wl,--gc-sections;-Wl,-z,max-page-size=16384")
''', encoding='utf-8')
    report = {'abi': options.abi, 'minimum_api': 28, 'ndk': options.ndk.name, 'sdk': str(sdk),
              'archive_bytes': archive.stat().st_size, 'build_commands_passed': len(checks),
              'scope': 'Prebuilt shared-widget application host and native services; application must define its widget factory'}
    (sdk / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report))


if __name__ == '__main__': main()

#!/usr/bin/env python3
"""Compile OneUI once, then assemble a fixed prebuilt SDK for app consumers.

Uses the installed NDK and already built, pinned Skia. Does not install apps or
download dependencies. Reuse the fixed build directory between invocations.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
from oneui_android_java_sdk import build_java_sdk, digest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ndk', type=Path, required=True)
    parser.add_argument('--sdk', type=Path, required=True)
    parser.add_argument('--abi', choices=['x86_64', 'arm64-v8a'], default='x86_64')
    parser.add_argument('--jobs', type=int, default=4, choices=range(1, 9))
    parser.add_argument('--work-root', type=Path, help='Isolated fixed native/SDK output directory')
    parser.add_argument('--skia-source', type=Path, help='Existing pinned Skia source checkout')
    parser.add_argument('--skia-build', type=Path, help='Matching existing Skia archives')
    options = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    work = options.work_root or root / 'out/mobile-current'
    if options.work_root:
        work.mkdir(parents=True, exist_ok=True)
        marker = work / 'owner.json'
        expected = {'owner': 'oneui-android-sdk-build', 'source': str(root.resolve())}
        if marker.is_file():
            if json.loads(marker.read_text(encoding='utf-8')) != expected:
                raise RuntimeError('Build directory belongs to another source checkout')
        elif any(work.iterdir()):
            raise RuntimeError('Refusing to use a nonempty unowned build directory')
        else:
            marker.write_text(json.dumps(expected, indent=2) + '\n', encoding='utf-8')
    build = work / ('android-' + options.abi)
    sdk = work / 'widget-sdk' / options.abi
    skia = options.skia_build or root / 'out/mobile-current' / ('skia-' + options.abi)
    skia_source = options.skia_source or root / 'third_party/skia'
    build.mkdir(parents=True, exist_ok=True); sdk.mkdir(parents=True, exist_ok=True)
    if not (skia / 'libskia.a').is_file(): raise RuntimeError('Build the matching pinned Android Skia first')
    checks = []

    def run(*command, data=None):
        subprocess.run([str(a) for a in command], input=data, text=True, check=True)
        checks.append(str(command[0]))

    run('cmake', '-S', root, '-B', build, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', '-DBUILD_TESTING=OFF',
        '-DONEUI_BUILD_ANDROID_APPLICATION_HOST=ON', '-DONEUI_BUILD_ANDROID_LAB=OFF',
        '-DONEUI_BUILD_VPN=ON', '-DONEUI_VPN_ANDROID_NATIVE=ON', '-DONEUI_BUILD_JSON=ON', '-DONEUI_ENABLE_YOGA=OFF',
        '-DONEUI_BUNDLED_SKIA_OUT=' + str(skia),
        '-DONEUI_BUNDLED_SKIA_ROOT=' + str(skia_source),
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
    for name, source in notices.items():
        path = skia_source / source.removeprefix('third_party/skia/') if source.startswith('third_party/skia/') else root / source
        shutil.copy2(path, licenses / name)
    java = build_java_sdk(root, options.sdk, build / 'java-sdk', sdk / 'oneui-android.jar')
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
              'archiveSHA256': digest(archive), 'java': java,
              'sourceCommit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
              'sourceDirty': bool(subprocess.check_output(['git', 'status', '--porcelain=v1', '--untracked-files=no'], cwd=root)),
              'skiaSourceCommit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=skia_source, text=True).strip(),
              'nativeInputs': {str(path): digest(path) for path in archives},
              'yyjsonInputs': {name: digest(root / 'third_party/yyjson' / name)
                               for name in ('src/yyjson.c', 'src/yyjson.h', 'LICENSE')},
              'scope': 'Prebuilt shared-widget application host and native services; application must define its widget factory'}
    (sdk / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({key: value for key, value in report.items() if key not in ('java', 'nativeInputs')}))


if __name__ == '__main__': main()

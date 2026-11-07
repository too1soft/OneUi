"""Build the pinned OneUI ARM64 SDK with the installed HarmonyOS API 26 NDK."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


SKIA_REVISION = '1f26101197bff9fcd939a791beb3094297436d59'
SKIA_ARCHIVES = ('skia', 'skparagraph', 'skshaper', 'skunicode_icu', 'skunicode_core',
                 'harfbuzz', 'icu', 'expat', 'jpeg', 'jpeg12', 'jpeg16', 'png',
                 'webp', 'skcms', 'zlib', 'freetype2')


def receipt(path, **fields):
    path.write_text(json.dumps({
        'status': 'pass', 'target': 'HarmonyOS', 'abi': 'arm64-v8a',
        'genuine_ohos_toolchain': True, **fields
    }, indent=2) + '\n', encoding='utf-8')


def arm64_hash(readelf, path):
    headers = subprocess.check_output([readelf, '-h', path], text=True)
    machines = [line.split(':', 1)[1].strip() for line in headers.splitlines() if 'Machine:' in line]
    if not machines or any(machine != 'AArch64' for machine in machines):
        raise RuntimeError(f'Not an AArch64 ELF or archive: {path}')
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build_skia(root, source, build, sdk, readelf, jobs):
    if subprocess.check_output(['git', '-C', source, 'rev-parse', 'HEAD'], text=True).strip() != SKIA_REVISION:
        raise RuntimeError('Skia must match the reviewed OneUI revision')
    subprocess.run(['cmake', f'-DONEUI_ICU_ROOT={source / "third_party/externals/icu"}',
                    '-P', root / 'scripts/apply-skia-patches.cmake'], check=True)
    build.mkdir(parents=True, exist_ok=True)
    # GN uses its POSIX rules, while Clang and the sysroot select the OHOS ABI
    # for every C, C++ and assembly object, including the ICU data object.
    target = ['--target=aarch64-linux-ohos', f'--sysroot={(sdk / "sysroot").as_posix()}']
    flags = {
        'target_os': 'linux', 'target_cpu': 'arm64', 'is_official_build': True,
        'is_debug': False, 'cc': 'clang', 'cxx': 'clang++', 'ar': 'llvm-ar',
        'extra_cflags': [*target, '-fPIC'], 'extra_asmflags': target, 'extra_ldflags': target,
        'skia_use_icu': True, 'skia_use_harfbuzz': True, 'skia_enable_skunicode': True,
        'skia_enable_skparagraph': True, 'skia_enable_skshaper': True,
    }
    for library in ('expat', 'harfbuzz', 'icu', 'freetype2', 'libjpeg_turbo', 'libpng', 'libwebp', 'zlib'):
        flags['skia_use_system_' + library] = False
    for feature in ('use_fontconfig', 'use_x11', 'enable_ganesh', 'use_gl', 'use_vulkan', 'use_dawn',
                    'use_perfetto', 'enable_pdf', 'enable_skottie', 'enable_svg', 'enable_tools',
                    'use_dng_sdk', 'use_wuffs', 'use_piex'):
        flags['skia_' + feature] = False
    (build / 'args.gn').write_text(''.join(f'{key}={json.dumps(value)}\n' for key, value in flags.items()), encoding='utf-8')
    env = dict(os.environ, PATH=str(sdk / 'llvm/bin') + os.pathsep + os.environ['PATH'])
    gn = source / 'bin' / ('gn.exe' if os.name == 'nt' else 'gn')
    subprocess.run([gn, 'gen', build, f'--root={source}', f'--script-executable={sys.executable}'], env=env, check=True)
    subprocess.run(['ninja', '-C', build, '-j', str(jobs), 'skia', 'skparagraph', 'skshaper', 'skunicode_icu'],
                   env=env, check=True)
    hashes = {f'lib{name}.a': arm64_hash(readelf, build / f'lib{name}.a') for name in SKIA_ARCHIVES}
    receipt(build / 'verification.json', skia_commit=SKIA_REVISION, target_triple=target[0].split('=', 1)[1],
            archives_sha256=hashes)


def build_sdk(root, output, sdk, skia, jobs):
    prefix = output / 'harmony-sdk/arm64-v8a'
    options = [
        f'-DCMAKE_TOOLCHAIN_FILE={sdk / "build/cmake/ohos.toolchain.cmake"}',
        '-DOHOS_ARCH=arm64-v8a', '-DOHOS_STL=c++_shared', '-DCMAKE_BUILD_TYPE=Release',
        f'-DCMAKE_INSTALL_PREFIX={prefix}', '-DBUILD_TESTING=OFF',
    ]
    packages = ('OneUIVpn', 'OneUIVpnJson', 'OneUIVpnPosix', 'OneUIJson')
    targets = (
        (root, 'harmony-native-vpn', ['-DONEUI_VPN_ONLY=ON', '-DONEUI_VPN_POSIX_NATIVE=ON', '-DONEUI_VPN_JSON=ON']),
        (root / 'platform/harmonyos/native', 'harmony-native-services',
         [f'-D{package}_DIR={prefix / "lib/cmake" / package}' for package in packages] +
         [f'-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={prefix / "lib"}']),
        (root, 'harmony-native-application',
         ['-DONEUI_BUILD_HARMONY_APPLICATION_HOST=ON', '-DONEUI_BUILD_JSON=ON',
          f'-DONEUI_BUNDLED_SKIA_OUT={skia}', '-DCMAKE_CXX_FLAGS=-Wno-unused-command-line-argument']),
    )
    for source, name, flags in targets:
        build = output / name
        subprocess.run(['cmake', '-S', source, '-B', build, '-G', 'Ninja', *options, *flags], check=True)
        subprocess.run(['cmake', '--build', build, '--parallel', str(jobs)], check=True)
        if name != 'harmony-native-services':
            subprocess.run(['cmake', '--install', build], check=True)
    shutil.copy2(sdk / 'llvm/lib/aarch64-linux-ohos/libc++_shared.so', prefix / 'lib/libc++_shared.so')
    return prefix


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', type=Path, required=True, help='DevEco SDK default/openharmony/native directory')
    parser.add_argument('--jobs', type=int, choices=range(1, 9), default=4)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    sdk = args.sdk.resolve(strict=True)
    if str(json.loads((sdk / 'oh-uni-package.json').read_text(encoding='utf-8'))['apiVersion']) != '26':
        raise RuntimeError('OneUI requires the official HarmonyOS API 26 NDK')
    output = root / 'out/mobile-current'
    if shutil.disk_usage(output).free < 22 * 1024**3:
        raise RuntimeError('Insufficient build volume headroom')
    skia = output / 'skia-harmony-arm64'
    readelf = sdk / 'llvm/bin' / ('llvm-readelf.exe' if os.name == 'nt' else 'llvm-readelf')
    build_skia(root, root / 'third_party/skia', skia, sdk, readelf, args.jobs)
    prefix = build_sdk(root, output, sdk, skia, args.jobs)
    libs = ('liboneui_application.so', 'liboneui_native_services.so', 'libc++_shared.so')
    receipt(prefix / 'verification.json', skia_receipt=str(skia / 'verification.json'),
            libraries_sha256={name: arm64_hash(readelf, prefix / 'lib' / name) for name in libs})
    subprocess.run([sys.executable, '-B', root / 'scripts/prepare-harmony-source-package.py'], check=True)


if __name__ == '__main__':
    main()

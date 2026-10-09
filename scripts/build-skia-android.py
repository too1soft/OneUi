"""Build the pinned Android Skia from existing sources and installed NDK.

No downloads. Outputs reuse out/mobile-current/skia-<ABI>. Check the project
artifact budget before running. This is for the dedicated OneUI SDK build job.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ndk', type=Path, required=True)
    parser.add_argument('--abi', choices=['x86_64', 'arm64-v8a'], default='x86_64')
    parser.add_argument('--jobs', type=int, choices=range(1, 9), default=3)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    source = root / 'third_party/skia'
    build = root / 'out/mobile-current' / ('skia-' + args.abi)
    revision = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip()
    if revision != '1f26101197bff9fcd939a791beb3094297436d59':
        raise RuntimeError('Skia does not match the reviewed OneUI dependency pin')
    ndk = args.ndk.resolve(strict=True)
    build.mkdir(parents=True, exist_ok=True)
    temporary = build / 'tmp'
    temporary.mkdir(exist_ok=True)
    os.environ['TEMP'] = os.environ['TMP'] = str(temporary)
    subprocess.run(['cmake', '-DONEUI_ICU_ROOT=' + str(source / 'third_party/externals/icu'),
                    '-P', str(root / 'scripts/apply-skia-patches.cmake')], check=True)
    flags = {
        'is_debug': False, 'is_official_build': True,
        'target_os': 'android', 'target_cpu': 'x64' if args.abi == 'x86_64' else 'arm64',
        'ndk': ndk.as_posix(), 'ndk_api': 28,
        'skia_use_system_expat': False, 'skia_use_system_harfbuzz': False,
        'skia_use_system_icu': False, 'skia_use_system_freetype2': False,
        'skia_use_system_libjpeg_turbo': False, 'skia_use_system_libpng': False,
        'skia_use_system_libwebp': False, 'skia_use_system_zlib': False,
        'skia_use_icu': True, 'skia_use_harfbuzz': True,
        'skia_enable_skparagraph': True, 'skia_enable_skshaper': True,
        'skia_enable_skunicode': True, 'skia_enable_ganesh': True,
        'skia_use_gl': True, 'skia_use_vulkan': False, 'skia_use_dawn': False,
        'skia_use_perfetto': False, 'skia_enable_pdf': False,
        'skia_enable_skottie': False, 'skia_enable_svg': False,
        'skia_enable_tools': False, 'skia_use_dng_sdk': False,
        'skia_use_wuffs': False, 'skia_use_piex': False,
        'skia_enable_fontmgr_android_ndk': False,
    }
    (build / 'args.gn').write_text(''.join(f'{k}={json.dumps(v)}\n' for k, v in flags.items()), encoding='utf-8')
    gn = source / 'bin' / ('gn.exe' if os.name == 'nt' else 'gn')
    subprocess.run([str(gn), 'gen', str(build), '--root=' + str(source),
                    '--script-executable=' + sys.executable], check=True)
    subprocess.run(['ninja', '-C', str(build), '-j', str(args.jobs),
                    'skia', 'skparagraph', 'skshaper', 'skunicode_icu'], check=True)


if __name__ == '__main__':
    main()

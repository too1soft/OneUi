#!/usr/bin/env python3
"""Compile the portable OneUI raster/N-API host using existing Linux Skia.

This is a Linux host check, never a HarmonyOS SDK or HAP producer.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--wsl-distro', default='Ubuntu')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    output = root / 'out/mobile-current/harmony-host'; output.mkdir(parents=True, exist_ok=True)
    linux = subprocess.check_output(['wsl', '-d', args.wsl_distro, '--exec', 'wslpath', '-a', root.as_posix()], text=True).strip()
    command = 'set -e\ncd ' + shlex.quote(linux) + '''
cmake -S . -B out/mobile-current/harmony-host -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel '-DCMAKE_CXX_FLAGS_MINSIZEREL=-Os -DNDEBUG -g0' -DONEUI_BUILD_HARMONY_APPLICATION_HOST=ON -DONEUI_HARMONY_HOST_TEST=ON -DONEUI_ENABLE_YOGA=OFF -DBUILD_TESTING=OFF -DONEUI_BUILD_JSON=ON -DONEUI_SKIA_MODE=bundled-static
cmake --build out/mobile-current/harmony-host --parallel 4 --target oneui_harmony_application oneui_json
'''
    result = subprocess.run(['wsl', '-d', args.wsl_distro, '--', 'bash', '-c', command], capture_output=True, text=True, encoding='utf-8', errors='replace')
    (output / 'build.log').write_text(result.stdout + result.stderr, encoding='utf-8')
    report = {'status': 'pass' if result.returncode == 0 else 'failed', 'target': 'Linux portable host test',
        'harmonyos_sdk_compiled': False, 'scope': __doc__,
        'source_sha256': {p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for directory in
            [root / 'src/platform/harmonyos', root / 'platform/harmonyos/native'] for p in directory.glob('*') if p.suffix in ['.cpp', '.h']}}
    (output / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    if result.returncode: raise RuntimeError((result.stdout + result.stderr)[-6000:])
    print(json.dumps({'status': 'pass', 'report': str(output / 'verification.json')}))


if __name__ == '__main__': main()

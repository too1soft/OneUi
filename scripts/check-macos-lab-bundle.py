#!/usr/bin/env python3
"""Verify the unified lab's sealed identity and local runtime dependency closure.

This validates a development bundle; it does not certify Developer ID signing,
notarization, notification delivery, or Gatekeeper acceptance.
"""
import argparse
import hashlib
import json
import plistlib
import re
import subprocess
from pathlib import Path


def run(*args):
    result = subprocess.run(args, capture_output=True, text=True, check=True)
    return result.stdout + result.stderr


def validate(bundle):
    identity = 'org.oneui.performance-lab'
    with (bundle / 'Contents/Info.plist').open('rb') as stream:
        info = plistlib.load(stream)
    if info.get('CFBundleIdentifier') != identity:
        raise ValueError('Bundle identifier differs from Application.id')
    if not info.get('CFBundleVersion') or not info.get('CFBundleShortVersionString'):
        raise ValueError('Bundle version is missing')
    run('/usr/bin/codesign', '--verify', '--deep', '--strict', str(bundle))
    signature = run('/usr/bin/codesign', '-dv', str(bundle))
    if f'Identifier={identity}\n' not in signature:
        raise ValueError('Code-signing identifier differs from the bundle identifier')
    executable = bundle / 'Contents/MacOS' / info['CFBundleExecutable']
    frameworks = bundle / 'Contents/Frameworks'
    binaries = [executable, frameworks / 'liboneui.dylib', frameworks / 'liboneui_lab_owner.dylib']
    records = []
    for binary in binaries:
        dependencies = [line.strip().split(' (')[0]
                        for line in run('/usr/bin/otool', '-L', str(binary)).splitlines()[1:]]
        for dependency in dependencies:
            if dependency.startswith(('/System/Library/', '/usr/lib/')):
                continue
            if not dependency.startswith('@rpath/') or not (frameworks / dependency[7:]).is_file():
                raise ValueError(f'Runtime dependency escapes the bundle: {dependency}')
        commands = run('/usr/bin/otool', '-l', str(binary))
        rpaths = re.findall(r'cmd LC_RPATH\s+cmdsize \d+\s+path (.*?) \(offset', commands)
        if any(path not in ('@executable_path/../Frameworks', '@loader_path') for path in rpaths):
            raise ValueError(f'Build-machine runtime search path: {rpaths}')
        if binary == executable and '@executable_path/../Frameworks' not in rpaths:
            raise ValueError('Executable cannot locate its bundled runtimes')
        records.append(dict(path=str(binary.relative_to(bundle)), bytes=binary.stat().st_size,
                            sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                            dependencies=dependencies, rpaths=rpaths))
    return dict(passed=True, bundle=str(bundle), identifier=identity,
                version=info['CFBundleVersion'], ad_hoc='Signature=adhoc' in signature,
                scope='Signature integrity and runtime closure only; not release certification',
                binaries=records)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bundle', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        result = validate(args.bundle.resolve())
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        result = dict(passed=False, error=str(error))
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(result, indent=2))
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())

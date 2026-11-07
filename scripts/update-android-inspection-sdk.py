"""Incrementally refresh the bounded semantic JNI bridge in existing SDKs.

Reuses the exact existing Ninja compiler command and replaces that single
archive member. No widget/Skia dependency rebuild or extra SDK copy is made.
"""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
ndk = Path(os.environ['LOCALAPPDATA']) / 'Android/Sdk/ndk/29.0.14206865'
archiver = ndk / 'toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-ar.exe'
member = 'application_text_android.cpp.o'
relative = 'CMakeFiles/oneui_android_application_host.dir/src/platform/android/' + member
source = root / 'src/platform/android/application_text_android.cpp'
header = root / 'include/oneui/mobile/widget_snapshot.h'

for abi in ('arm64-v8a', 'x86_64'):
    build = root / 'out/mobile-current' / ('android-' + abi)
    sdk = root / 'out/mobile-current/widget-sdk' / abi
    archive = sdk / 'lib/liboneui_android_application.a'
    if not archive.is_file() or shutil.disk_usage(build).free < 21 * 1024**3:
        raise RuntimeError('Existing SDK and disk headroom required')
    command = subprocess.check_output(['ninja', '-C', str(build), '-t', 'commands', relative], text=True)
    matches = [line for line in command.splitlines() if ' -c ' in line and str(source.as_posix()) in line]
    if len(matches) != 1 or 'clang++.exe' not in matches[0] or '&&' in matches[0]:
        raise RuntimeError('Expected one direct, existing native compiler invocation')
    # Windows CreateProcess parses the original Ninja command; no shell runs.
    subprocess.run(matches[0], cwd=build, shell=False, check=True)
    members_before = subprocess.check_output([str(archiver), 't', str(archive)], text=True).splitlines()
    if members_before.count(member) != 1:
        raise RuntimeError('Expected exactly one JNI inspection archive member')
    subprocess.run([str(archiver), 'rcs', str(archive), str(build / relative)], check=True)
    members_after = subprocess.check_output([str(archiver), 't', str(archive)], text=True).splitlines()
    if members_after != members_before:
        raise RuntimeError('Archive member set/order changed')
    shutil.copy2(header, sdk / 'include/oneui/mobile/widget_snapshot.h')
    receipt = sdk / 'verification.json'
    metadata = json.loads(receipt.read_text(encoding='utf-8-sig'))
    metadata['semantic_inspection'] = {
        'header_sha256': hashlib.sha256(header.read_bytes()).hexdigest(),
        'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
        'archive_sha256': hashlib.sha256(archive.read_bytes()).hexdigest(),
        'recompiled_members': [member], 'dependencies_rebuilt': False,
    }
    receipt.write_text(json.dumps(metadata, indent=2) + '\n', encoding='utf-8')
print(json.dumps({'status': 'pass', 'abis': ['arm64-v8a', 'x86_64'], 'members_per_abi': 1}))

#!/usr/bin/env python3
"""Refresh ABI-independent Java services in existing prebuilt Android SDKs."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import zipfile

root = Path(__file__).resolve().parents[1]
output = root / 'out/mobile-current/android-x86_64/java-sdk'
output.mkdir(parents=True, exist_ok=True)
sdk = Path(os.environ['LOCALAPPDATA']) / 'Android/Sdk'
jar = sdk / 'platforms/android-36/android.jar'
sources = sorted((root / 'platform/android/java').rglob('*.java'))
before = {p: p.stat().st_mtime_ns for p in output.rglob('*.class')}
subprocess.run([str(Path(os.environ['JAVA_HOME']) / 'bin/javac.exe'), '-encoding', 'UTF-8', '-source', '8', '-target', '8',
    '-bootclasspath', str(jar) + ';' + str(sdk / 'build-tools/36.1.0/core-lambda-stubs.jar'), '-d', str(output),
    *map(str, sources)], check=True)
classes = [p for p in output.rglob('*.class') if p not in before or p.stat().st_mtime_ns != before[p]]
if not classes:
    raise RuntimeError('No current SDK classes emitted')
for abi in ['arm64-v8a', 'x86_64']:
    target = root / 'out/mobile-current/widget-sdk' / abi
    receipt = target / 'verification.json'
    metadata = json.loads(receipt.read_text(encoding='utf-8-sig'))
    if not (target / 'lib/liboneui_android_application.a').is_file():
        raise RuntimeError('Existing native SDK required')
    with zipfile.ZipFile(target / 'oneui-android.jar', 'w', zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(classes):
            archive.write(path, path.relative_to(output).as_posix())
    metadata['java_services'] = {'current_source_sha256': hashlib.sha256(b''.join(p.read_bytes() for p in sources)).hexdigest(),
        'classes': len(classes), 'native_archive_rebuilt': False}
    receipt.write_text(json.dumps(metadata, indent=2) + '\n', encoding='utf-8')
print(json.dumps({'status': 'pass', 'classes': len(classes), 'abis': ['arm64-v8a', 'x86_64']}))

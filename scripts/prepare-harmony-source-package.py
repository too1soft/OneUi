#!/usr/bin/env python3
"""Prepare a fixed OneUI HAR source package; never call this a compiled HAR.

Native libraries are copied only from a verified genuine HarmonyOS SDK. Missing
SDK inputs remain missing; no Linux/Android binary or stub is substituted.
"""
import hashlib
import json
from pathlib import Path
import shutil


def main():
    root = Path(__file__).resolve().parents[1]
    source = root / 'platform/harmonyos'
    output = root / 'out/mobile-current/harmony-har'
    output.mkdir(parents=True, exist_ok=True)
    hashes = {}
    for category in ['application', 'nativebridge', 'storage', 'vpn']:
        for item in (source / category).rglob('*.ets'):
            relative = item.relative_to(source)
            target = output / 'src/main/ets' / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(item, target)
            hashes[relative.as_posix()] = hashlib.sha256(item.read_bytes()).hexdigest()
    index = (source / 'Index.ets').read_text(encoding='utf-8')
    for category in ['application', 'nativebridge', 'storage', 'vpn']:
        index = index.replace("'./" + category + '/', "'./src/main/ets/" + category + '/')
    (output / 'Index.ets').write_text(index, encoding='utf-8')
    shutil.copytree(source / 'native/types', output / 'src/main/cpp/types', dirs_exist_ok=True)
    def write(name, value):
        path = output / name; path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    write('oh-package.json5', {'name': '@too1/oneui', 'version': '0.1.0', 'main': 'Index.ets', 'license': 'MIT',
        'dependencies': {name: 'file:./src/main/cpp/types/' + name.removesuffix('.so') for name in ['liboneui_application.so', 'liboneui_native_services.so']}})
    write('src/main/module.json5', {'module': {'name': 'oneui', 'type': 'har', 'deviceTypes': ['phone', 'tablet']}})
    write('build-profile.json5', {'apiType': 'stageMode', 'targets': [{'name': 'default'}]})
    (output / 'hvigorfile.ts').write_text("import { harTasks } from '@ohos/hvigor-ohos-plugin';\nexport default { system: harTasks, plugins: [] };\n", encoding='utf-8')
    shutil.copy2(root / 'LICENSE', output / 'LICENSE')
    sdk = root / 'out/mobile-current/harmony-sdk/arm64-v8a'
    names = ['liboneui_application.so', 'liboneui_native_services.so', 'libc++_shared.so']
    receipt = sdk / 'verification.json'
    verified = False
    if receipt.is_file():
        data = json.loads(receipt.read_text(encoding='utf-8-sig'))
        verified = data.get('status') == 'pass' and data.get('target') == 'HarmonyOS' and data.get('genuine_ohos_toolchain') is True
    missing = [str(sdk / 'lib' / name) for name in names if not verified or not (sdk / 'lib' / name).is_file()]
    if not missing:
        native = output / 'libs/arm64-v8a'; native.mkdir(parents=True, exist_ok=True)
        for name in names: shutil.copy2(sdk / 'lib' / name, native / name)
    report = {'status': 'source_prepared', 'source_package': str(output), 'source_sha256': hashes,
        'compiled_har': False, 'genuine_ohos_native_libraries_ready': not missing, 'missing': missing,
        'scope': 'OneUI source package and native type metadata only; target SDK/ArkTS/HAP/device acceptance is separate'}
    (output / 'preparation.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'status': report['status'], 'sources': len(hashes), 'native_libraries_ready': not missing, 'report': str(output / 'preparation.json')}))


if __name__ == '__main__': main()

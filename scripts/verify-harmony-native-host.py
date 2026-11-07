#!/usr/bin/env python3
"""Compile the Harmony N-API boundary on Linux; explicitly mock OS services.

Uses existing WSL GCC/Node headers, a fixed output directory and no Go cache.
This checks portable native behavior, not HarmonyOS SDK/HAP/device acceptance.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--wsl-distro', default='Ubuntu-22.04')
    parser.add_argument('--typescript-root', type=Path)
    options = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    output = root / 'out/mobile-current/posix-services'; output.mkdir(parents=True, exist_ok=True)
    linux = subprocess.check_output(['wsl', '-d', options.wsl_distro, '--exec', 'wslpath', '-a', root.as_posix()], text=True).strip()
    base = 'set -e\ncd ' + shlex.quote(linux) + '\n'
    checks = []
    def run(name, command):
        result = subprocess.run(['wsl', '-d', options.wsl_distro, '--', 'bash', '-c', base + command], capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=120)
        checks.append({'name': name, 'exit': result.returncode, 'output': (result.stdout + result.stderr)[-4000:]})
        if result.returncode: raise RuntimeError(name + ': ' + checks[-1]['output'])
    prefix = 'out/mobile-current/posix-services/'
    flags = '-std=c++17 -O1 -Wall -Wextra -Werror -Iinclude -Ibindings/go/oneuivpn/native -pthread '
    sources = ['platform/harmonyos/native/' + x + '.cpp' for x in ['module', 'napi_support', 'native_module_napi', 'native_callbacks', 'native_port_napi', 'native_session_napi', 'native_blob_files_napi', 'native_network_napi', 'native_channel_napi']]
    sources += ['src/vpn/' + x + '.cpp' for x in ['backend_posix', 'posix_packet_pump', 'config', 'config_json', 'session', 'c_api']]
    sources += ['src/mobile/native_service_queue.cpp', 'src/mobile/native_module_posix.cpp', 'src/mobile/atomic_blob_files.cpp', 'src/mobile/private_directory.cpp', 'src/mobile/local_channel.cpp', 'src/data/json.cpp']
    status = 'failed'
    try:
        compiler = options.typescript_root or root / 'website'
        subprocess.run(['node', str(root / 'scripts/compile-ets-host-check.cjs'),
            str(root / 'platform/harmonyos/storage/AuthenticatedBlobStore.ets'), str(output / 'authenticated-store.cjs'), str(compiler)], check=True)
        subprocess.run(['node', str(root / 'scripts/compile-ets-host-check.cjs'),
            str(root / 'platform/harmonyos/vpn/OneUiPacketTunnel.ets'), str(output / 'packet-tunnel.cjs'), str(compiler)], check=True)
        subprocess.run(['node', str(root / 'scripts/compile-ets-host-check.cjs'),
            str(root / 'platform/harmonyos/nativebridge/OneUiNativeWork.ets'), str(output / 'native-work.cjs'), str(compiler)], check=True)
        subprocess.run(['node', str(root / 'scripts/compile-ets-host-check.cjs'),
            str(root / 'platform/harmonyos/nativebridge/OneUiNativeTaskScope.ets'), str(output / 'native-task-scope.cjs'), str(compiler)], check=True)
        run('compile yyjson', 'gcc -O1 -fPIC -Ithird_party/yyjson/src -c third_party/yyjson/src/yyjson.c -o ' + prefix + 'yyjson.o')
        run('compile N-API host boundary', 'g++ ' + flags + '-fPIC -shared -DONEUI_NODE_TEST -DONEUI_VPN_STATIC -I/usr/include/node -Ithird_party/yyjson/src ' + ' '.join(sources) + ' ' + prefix + 'yyjson.o -ldl -o ' + prefix + 'oneui_native_services.node')
        run('compile isolated callback fixture', 'g++ ' + flags + '-fPIC -shared tests/vpn/harmony_native_fixture.cpp -o ' + prefix + 'liboneui_harmony_fixture.so')
        run('N-API worker/service/session behavior', 'LD_LIBRARY_PATH=' + shlex.quote(linux + '/' + prefix) + ' node tests/vpn/harmony_native_checks.cjs ' + prefix + 'oneui_native_services.node')
        run('POSIX packet ownership', 'g++ ' + flags + 'src/vpn/posix_packet_pump.cpp tests/vpn/posix_packet_tests.cpp -o ' + prefix + 'packet-tests\n' + prefix + 'packet-tests')
        run('native callback cancellation and bounds', 'g++ ' + flags + 'src/mobile/native_service_queue.cpp tests/mobile/native_service_queue_tests.cpp -o ' + prefix + 'queue-tests\n' + prefix + 'queue-tests')
        run('authenticated blob policy and actual private files', 'node tests/vpn/harmony_storage_checks.cjs ' + prefix + 'oneui_native_services.node ' + prefix + 'authenticated-store.cjs')
        run('private native channel across actual processes', 'node tests/vpn/harmony_channel_checks.cjs ' + prefix + 'oneui_native_services.node')
        run('system tunnel rollback and cancellation', 'node tests/vpn/harmony_tunnel_checks.cjs ' + prefix + 'packet-tunnel.cjs')
        linux_compiler = subprocess.check_output(['wsl', '-d', options.wsl_distro, '--exec', 'wslpath', '-a', compiler.as_posix()], text=True).strip()
        run('ArkTS scheduling and late OS cleanup ownership (explicit mocks)', 'node tests/vpn/harmony_async_checks.cjs ' + shlex.quote(linux_compiler))
        run('actual ArkTS channel/RPC on native private sockets', 'node tests/vpn/harmony_rpc_checks.cjs ' + prefix + 'oneui_native_services.node ' + shlex.quote(linux_compiler))
        run('actual ArkTS VPN lifecycle with C++ state/native IPC (OS mocked)', 'node tests/vpn/harmony_runtime_checks.cjs ' + prefix + 'oneui_native_services.node ' + shlex.quote(linux_compiler))
        run('native task ownership, cancellation and cleanup retry', 'node tests/vpn/harmony_task_scope_checks.cjs ' + prefix + 'native-task-scope.cjs')
        status = 'pass'
    finally:
        report = {'status': status, 'checks': checks, 'scope': __doc__, 'harmonyos_sdk_compiled': False,
            'os_services': 'explicit test mocks; no HUKS/AssetStore, VPN permission or device traffic claimed',
            'temporary_root': '~/.cache/oneui-mobile-native-fixture on selected WSL; marker/owner/path guarded cleanup in finally',
            'sources': {name: hashlib.sha256((root / name).read_bytes()).hexdigest() for name in sources}}
        (output / 'verification.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'status': status, 'checks': len(checks), 'report': str(output / 'verification.json')}))


if __name__ == '__main__': main()

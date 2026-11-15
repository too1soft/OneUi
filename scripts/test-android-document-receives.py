"""Build/run owned SAF receive, restart, lease and publication recovery contracts."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SDK = Path(os.environ['LOCALAPPDATA']) / 'Android/sdk'
TOOLS = SDK / 'build-tools/36.1.0'
JDK = Path('C:/Program Files/Eclipse Adoptium/jdk-17.0.18.8-hotspot/bin')
PACKAGE = 'net.too1.oneui.documents.receivecontracts'
RUNNER = PACKAGE + '/net.too1.oneui.documents.DocumentReceiveContractRunner'


def run(args, timeout=45, check=True):
    result = subprocess.run(list(map(str, args)), check=False, capture_output=True,
                            encoding='utf-8', errors='replace', timeout=timeout)
    if check and result.returncode:
        print(result.stdout + result.stderr, flush=True)
        result.check_returncode()
    return result


def build(output):
    classes, dex = output / 'classes', output / 'dex'
    dex.mkdir(parents=True, exist_ok=False)
    sources = sorted((ROOT / 'platform/android/documents/java').rglob('*.java'))
    sources += sorted((ROOT / 'tests/mobile/android-document-receives').glob('*.java'))
    android = SDK / 'platforms/android-36/android.jar'
    run([JDK / 'javac.exe', '-J-Duser.language=en', '-J-Duser.country=US', '-encoding', 'UTF-8', '-source', '17', '-target', '17',
         '-classpath', android, '-d', classes, *sources])
    run([JDK / 'java.exe', '-cp', TOOLS / 'lib/d8.jar', 'com.android.tools.r8.D8', '--min-api', '28',
         '--lib', android, '--output', dex, *classes.rglob('*.class')])
    manifest = output / 'AndroidManifest.xml'
    manifest.write_text('''<manifest xmlns:android="http://schemas.android.com/apk/res/android" package="net.too1.oneui.documents.receivecontracts">
<uses-sdk android:minSdkVersion="28" android:targetSdkVersion="36"/>
<application android:label="Owned receive contracts" android:allowBackup="false" android:debuggable="true">
<service android:name="net.too1.oneui.documents.LeaseProbeService" android:exported="false" android:process=":leaseprobe"/>
<provider android:name="net.too1.oneui.documents.OwnedReceiveProvider" android:authorities="net.too1.oneui.documents.receivefixture"
 android:exported="true" android:grantUriPermissions="true" android:permission="android.permission.MANAGE_DOCUMENTS">
</provider>
</application>
<instrumentation android:name="net.too1.oneui.documents.DocumentReceiveContractRunner" android:targetPackage="net.too1.oneui.documents.receivecontracts"/>
</manifest>''', encoding='utf-8')
    unsigned, aligned, signed = [output / name for name in ('unsigned.apk', 'aligned.apk', 'receive-contracts.apk')]
    run([TOOLS / 'aapt2.exe', 'link', '-o', unsigned, '--manifest', manifest, '-I', android])
    with zipfile.ZipFile(unsigned, 'a') as archive:
        archive.write(dex / 'classes.dex', 'classes.dex')
    run([TOOLS / 'zipalign.exe', '-P', '16', '-f', '4', unsigned, aligned])
    run([JDK / 'java.exe', '-jar', TOOLS / 'lib/apksigner.jar', 'sign', '--ks', Path.home() / '.android/debug.keystore',
         '--ks-pass', 'pass:android', '--key-pass', 'pass:android', '--out', signed, aligned])
    run([JDK / 'java.exe', '-jar', TOOLS / 'lib/apksigner.jar', 'verify', signed])
    proof = {'sources': [{'path': p.relative_to(ROOT).as_posix(), 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()} for p in sources],
             'apk_sha256': hashlib.sha256(signed.read_bytes()).hexdigest(),
             'fixture_provider': 'same UID DocumentsProvider; synthetic local grant; no system SAF picker'}
    (output / 'build-proof.json').write_text(json.dumps(proof, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'sources': len(sources), 'apk_sha256': proof['apk_sha256']}), flush=True)


def execute(output, serial):
    def adb(*args, check=True):
        return run([SDK / 'platform-tools/adb.exe', '-s', serial, *args], check=check)
    assert adb('shell', 'id', '-u').stdout.strip() == '2000', 'ordinary ADB UID required'
    result_dir = output / 'run-results'
    result_dir.mkdir(exist_ok=False)
    apk = output / 'receive-contracts.apk'
    assert 'Success' in adb('install', '-r', apk).stdout
    installed = adb('shell', 'pm', 'path', PACKAGE).stdout.strip().removeprefix('package:')
    assert adb('shell', 'sha256sum', installed).stdout.split()[0] == hashlib.sha256(apk.read_bytes()).hexdigest()
    assert 'Success' in adb('shell', 'pm', 'clear', PACKAGE).stdout
    phases = []

    def phase(name, expect_crash=False):
        try:
            result = adb('shell', 'am', 'instrument', '-w', '-r', '-e', 'phase', name, RUNNER, check=False)
        except subprocess.TimeoutExpired as error:
            partial = (error.stdout or b'') + (error.stderr or b'')
            (result_dir / (name + '-timeout.log')).write_bytes(partial)
            raise
        (result_dir / (name + '.log')).write_text(result.stdout + result.stderr, encoding='utf-8')
        fields = dict(re.findall(r'^INSTRUMENTATION_RESULT: ([^=]+)=(.*)$', result.stdout, re.MULTILINE))
        entry = {'phase': name, 'exit_code': result.returncode, 'fields': fields, 'expected_process_death': expect_crash}
        phases.append(entry)
        (result_dir / 'results.json').write_text(json.dumps(phases, indent=2) + '\n', encoding='utf-8')
        print(json.dumps(entry), flush=True)
        if expect_crash:
            assert fields.get('passed') != 'true', 'publication crash fixture did not terminate'
        else:
            assert 'INSTRUMENTATION_CODE: -1' in result.stdout and fields.get('passed') == 'true', 'receive phase failed: ' + name

    phase('contracts')
    phase('prepare-resume')
    adb('shell', 'am', 'force-stop', PACKAGE)
    phase('resume')
    # This phase must be observed alive with an open data FD before force-stop.
    marker = 'files/hold-ready.json'
    adb('shell', 'run-as', PACKAGE, 'rm', '-f', marker)
    with (result_dir / 'hold-open.log').open('wb') as log:
        process = subprocess.Popen([str(SDK / 'platform-tools/adb.exe'), '-s', serial, 'shell', 'am', 'instrument',
                                    '-w', '-r', '-e', 'phase', 'hold-open', RUNNER], stdout=log, stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic() + 20
            while time.monotonic() < deadline:
                found = adb('shell', 'run-as', PACKAGE, 'cat', marker, check=False)
                if found.returncode == 0:
                    token = json.loads(found.stdout)
                    assert token.get('fd', -1) >= 0 and token.get('descriptor')
                    assert process.poll() is None and adb('shell', 'pidof', PACKAGE).stdout.strip()
                    (result_dir / 'hold-ready.json').write_text(found.stdout, encoding='utf-8')
                    break
                assert process.poll() is None, 'hold process exited before marker'
                time.sleep(0.2)
            else:
                raise AssertionError('hold process did not publish live marker')
        finally:
            adb('shell', 'am', 'force-stop', PACKAGE)
            process.wait(timeout=15)
    # Store repeated phases separately rather than replacing successful evidence.
    previous = result_dir
    result_dir = output / 'run-results-after-kill'
    result_dir.mkdir(exist_ok=False)
    phase('resume')
    phase('crash-opaque', True)
    phase('recover-opaque')
    phase('crash-changing', True)
    phase('recover-changing')
    proof = {'passed': True, 'ordinary_uid': 2000, 'phases': phases,
             'provider': 'owned same-UID; no product UI, external provider or actual system grant',
             'live_force_stop_marker': str(previous / 'hold-ready.json')}
    (output / 'result.json').write_text(json.dumps(proof, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'passed': True, 'phases': len(phases)}), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=['build', 'run'])
    parser.add_argument('--stage', type=Path, required=True)
    parser.add_argument('--attempt', type=int, required=True)
    parser.add_argument('--serial')
    args = parser.parse_args()
    stage = args.stage.resolve(strict=True)
    assert (stage / '.owner').read_text().strip() == 'wangyunkong-go-saf-receive'
    assert args.attempt > 0
    output = stage / str(args.attempt)
    if args.mode == 'build':
        output.mkdir(exist_ok=False)
        build(output)
    else:
        assert args.serial and re.fullmatch(r'emulator-[0-9]+', args.serial)
        execute(output, args.serial)


if __name__ == '__main__':
    main()

"""Native process-local trust regression. Does not install anything or sign approvals.

Only verifier processes execute; test subjects (including corrupted files) are
never launched. Record guest OS, root stores and every input/output hash.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('vbox', 'vm', 'username', 'password-file', 'probe', 'signed-subject', 'signer-sha256', 'output'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    pin = args.signer_sha256.lower()
    if len(pin) != 64 or any(ch not in '0123456789abcdef' for ch in pin):
        raise ValueError('An independently trusted certificate SHA256 is required')
    output = Path(args.output).resolve()
    output.mkdir(parents=True, exist_ok=False)
    stage = output / 'inputs'
    stage.mkdir()
    probe = stage / 'process-trust-probe.exe'
    shutil.copyfile(args.probe, probe)
    shutil.copyfile(args.signed_subject, stage / 'signed-subject.exe')
    shutil.copyfile(args.probe, stage / 'unsigned-subject.exe')
    data = bytearray((stage / 'signed-subject.exe').read_bytes())
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    if data[pe:pe+4] != b'PE\x00\x00':
        raise ValueError('Expected a PE test subject')
    optional_size = struct.unpack_from('<H', data, pe+20)[0]
    section = pe + 24 + optional_size
    raw_size, raw_offset = struct.unpack_from('<II', data, section+16)
    if raw_size <= 64 or raw_offset + raw_size > len(data):
        raise ValueError('Test subject has no safe raw-section corruption offset')
    data[raw_offset+64] ^= 1
    (stage / 'tampered-subject.exe').write_bytes(data)
    del data
    harness = Path(__file__).with_name('run-vm-probe.py')

    def observe(filename):
        # Native API only; no PowerShell or system-update prerequisite.
        directory = output / (filename+'.evidence')
        command = [sys.executable, str(harness), '--vbox', args.vbox, '--vm', args.vm,
            '--username', args.username, '--password-file', args.password_file,
            '--binary', str(probe), '--output', str(directory),
            '--argument=--environment', '--timeout-seconds', '30']
        run = subprocess.run(command, capture_output=True, timeout=100)
        path = output / filename
        if (directory/'stdout.txt').exists():
            shutil.copyfile(directory/'stdout.txt', path)
        if run.returncode != 0:
            raise RuntimeError(f'Guest root-store observation failed ({run.returncode}); preserved stdout/stderr')
        records = [json.loads(line) for line in path.read_text(encoding='utf-8').splitlines() if line.startswith('{')]
        if len(records) != 1:
            raise RuntimeError('Native environment result is missing or ambiguous')
        record = records[0]
        if (record.get('major'), record.get('minor'), record.get('build'), record.get('servicePack'), record.get('nativeArchitecture'), record.get('processBits')) != (6, 1, 7601, 1, 0, 32):
            raise ValueError('This native regression requires the original Win7 SP1 x86 guest')
        return record

    before = observe('system-before.txt')
    cases = [
        ('signed', 'signed-subject.exe', pin, '0x00000000'),
        ('tampered', 'tampered-subject.exe', pin, '0x80096010'),
        ('unsigned', 'unsigned-subject.exe', pin, '0x800b0100'),
        ('wrong-publisher', 'signed-subject.exe', '0'*64, '0x800b0004'),
        ('signed-after-failures', 'signed-subject.exe', pin, '0x00000000'),
    ]
    results = []
    for name, subject, case_pin, expected in cases:
        directory = output / name
        command = [sys.executable, str(harness), '--vbox', args.vbox, '--vm', args.vm,
            '--username', args.username, '--password-file', args.password_file,
            '--binary', str(probe), '--output', str(directory), '--file', subject,
            '--argument', subject, '--argument', case_pin, '--timeout-seconds', '120']
        run = subprocess.run(command, capture_output=True, timeout=200)
        if not (directory/'stdout.txt').exists():
            raise RuntimeError(f'Native test transport failed: {name}')
        records = [json.loads(line) for line in (directory/'stdout.txt').read_text(encoding='utf-8').splitlines() if line.startswith('{')]
        final = [r for r in records if r.get('stage') == 'complete']
        if len(final) != 1:
            raise RuntimeError(f'Native test did not complete: {name}')
        actual = final[0]
        # VBoxManage reports a normal nonzero guest exit as 32 + exit status.
        # Do not confuse this with transport failures, crashes or a timeout.
        expected_transport_exit = 0 if expected == '0x00000000' else 33
        matched = actual.get('code') == expected and actual.get('passed') == (expected == '0x00000000') and run.returncode == expected_transport_exit
        results.append(dict(case=name, expectedCode=expected, actual=actual,
            transportExitCode=run.returncode, expectedTransportExitCode=expected_transport_exit,
            matched=matched, subjectSHA256=sha(stage/subject),
            evidenceSHA256=sha(directory/'result.json')))
        print(json.dumps(results[-1]), flush=True)
    after = observe('system-after.txt')
    unchanged = before == after
    result = dict(schemaVersion=1, time=datetime.now(timezone.utc).isoformat(),
        scope='native-process-local-authenticode-regression-not-installation-acceptance',
        vm=args.vm, probeSHA256=sha(probe), signerSHA256=pin,
        systemBeforeSHA256=sha(output/'system-before.txt'),
        systemAfterSHA256=sha(output/'system-after.txt'), rootStoresUnchanged=unchanged,
        cases=results, passed=unchanged and all(r['matched'] for r in results))
    (output/'regression.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    sys.exit(main())

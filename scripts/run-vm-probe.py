"""Capture native smoke evidence in an existing isolated, explicitly named VM."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import uuid
from datetime import datetime, timezone
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from compatlib import sha256

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--vbox', required=True)
p.add_argument('--vm', required=True)
p.add_argument('--username', required=True)
p.add_argument('--password-file', required=True)
p.add_argument('--binary', required=True)
p.add_argument('--output', required=True)
p.add_argument('--argument', action='append', default=[])
p.add_argument('--dependency', action='append', default=[], help='explicit adjacent runtime file, never discovered from PATH')
p.add_argument('--file', action='append', default=[], help='explicit adjacent test input, never executed by the harness')
p.add_argument('--timeout-seconds', type=int, default=45)
p.add_argument('--env', action='append', default=[], help='test-only guest environment NAME=value')
a = p.parse_args()
if not 1 <= a.timeout_seconds <= 240:
    raise ValueError('probe timeout must be between 1 and 240 seconds')
output = Path(a.output)
if output.exists():
    raise ValueError('acceptance output already exists')
output.mkdir(parents=True)
binary = Path(a.binary).resolve()
guest_root = 'C:\\Users\\' + a.username + '\\Desktop\\compat-probe-' + uuid.uuid4().hex
guest = guest_root + '\\' + binary.name
auth = ['--username', a.username, '--passwordfile', a.password_file]
base = [a.vbox, 'guestcontrol', a.vm]
subprocess.run(base + ['mkdir', *auth, guest_root], check=True, capture_output=True, timeout=60)
dependencies = {}
for name in a.dependency:
    if Path(name).name != name or not name.lower().endswith('.dll'):
        raise ValueError('dependency must name an adjacent DLL')
    path = binary.parent / name
    if path.is_symlink() or not path.is_file():
        raise ValueError('dependency is missing or linked')
    dependencies[name] = sha256(path)
    subprocess.run(base + ['copyto', *auth, str(path), guest_root + '\\' + name], check=True, capture_output=True, timeout=60)
inputs = {}
for name in a.file:
    if Path(name).name != name or name.lower() in {binary.name.lower(), *[n.lower() for n in dependencies], *[n.lower() for n in inputs]}:
        raise ValueError('test input must have a distinct adjacent filename')
    path = binary.parent / name
    if path.is_symlink() or not path.is_file():
        raise ValueError('test input is missing or linked')
    inputs[name] = sha256(path)
    subprocess.run(base + ['copyto', *auth, str(path), guest_root + '\\' + name], check=True, capture_output=True, timeout=60)
subprocess.run(base + ['copyto', *auth, str(binary), guest], check=True, capture_output=True, timeout=60)
arguments = [guest_root + '\\' + item if item in inputs else item for item in a.argument]
run = subprocess.run(base + ['run', *auth, '--exe', guest, '--timeout', str(a.timeout_seconds * 1000), '--wait-stdout', '--wait-stderr', *['--putenv='+v for v in a.env], '--', *arguments], capture_output=True, timeout=a.timeout_seconds + 10)
(output / 'stdout.txt').write_bytes(run.stdout)
(output / 'stderr.txt').write_bytes(run.stderr)
record = {'time': datetime.now(timezone.utc).isoformat(), 'vm': a.vm, 'binary': binary.name,
          'sha256': sha256(binary), 'dependencies': dependencies, 'inputs': inputs, 'exitCode': run.returncode, 'arguments': a.argument,
          'scope': 'native-smoke-only', 'stdoutSha256': sha256(output/'stdout.txt'),
          'stderrSha256': sha256(output/'stderr.txt')}
(output / 'result.json').write_text(json.dumps(record, indent=2), encoding='utf-8')
print(json.dumps(record))
sys.exit(run.returncode)

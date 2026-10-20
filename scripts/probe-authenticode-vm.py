"""Read-only native Authenticode check in an explicitly named disposable VM.

Uses the PowerShell 2.0 cmdlet already supplied by Win7. It never installs a
certificate, changes execution policy, or treats host verification as native.
"""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import subprocess
import sys
import uuid
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from compatlib import sha256

p = argparse.ArgumentParser(description=__doc__)
for name in ('vbox', 'vm', 'username', 'password-file', 'binary', 'output'):
    p.add_argument('--' + name, required=True)
p.add_argument('--timeout-seconds', type=int, default=120)
a = p.parse_args()
if not 30 <= a.timeout_seconds <= 240:
    raise ValueError('timeout must be between 30 and 240 seconds')
output = Path(a.output)
if output.exists():
    raise ValueError('refusing to overwrite evidence')
output.mkdir(parents=True)
binary = Path(a.binary).resolve()
guest = 'C:\\Users\\' + a.username + '\\Desktop\\signature-probe-' + uuid.uuid4().hex + '.exe'
if "'" in guest or '\n' in guest:
    raise ValueError('invalid guest path')
auth = ['--username', a.username, '--passwordfile', a.password_file]
base = [a.vbox, 'guestcontrol', a.vm]
subprocess.run(base + ['copyto', *auth, str(binary), guest], capture_output=True, check=True, timeout=60)
command = ("$ErrorActionPreference='Stop'; $p='" + guest + "'; "
    "Write-Output 'Stage=signature-verification'; $s=Get-AuthenticodeSignature -FilePath $p; "
    "Write-Output ('Status='+$s.Status); Write-Output ('Message='+$s.StatusMessage); "
    "Write-Output ('Signer='+$s.SignerCertificate.Thumbprint); Write-Output 'Stage=system-information'; $os=[Environment]::OSVersion; "
    "$h=[Security.Cryptography.SHA256]::Create(); $f=[IO.File]::OpenRead($p); "
    "try { $digest=([BitConverter]::ToString($h.ComputeHash($f))).Replace('-','').ToLowerInvariant() } finally { $f.Close(); $h.Dispose() }; "
    "Write-Output ('FileSHA256='+$digest); Write-Output ('Status='+$s.Status); "
    "Write-Output ('OS='+$os.Version); Write-Output ('SP='+$os.ServicePack); "
    "Write-Output ('ProcessBits='+([IntPtr]::Size*8)); "
    "if ($s.Status -eq 'Valid') { exit 0 } else { exit 1 }")
run = subprocess.run(base + ['run', *auth, '--exe', r'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe',
    '--timeout', str(a.timeout_seconds * 1000), '--wait-stdout', '--wait-stderr', '--', '-NoProfile', '-NonInteractive', '-Command', command],
    capture_output=True, timeout=a.timeout_seconds + 10)
(output/'stdout.txt').write_bytes(run.stdout)
(output/'stderr.txt').write_bytes(run.stderr)
record = dict(time=datetime.now(timezone.utc).isoformat(), vm=a.vm, binary=binary.name,
    sha256=sha256(binary), exitCode=run.returncode, scope='native-authenticode-only',
    stdoutSha256=sha256(output/'stdout.txt'), stderrSha256=sha256(output/'stderr.txt'))
(output/'result.json').write_text(json.dumps(record, indent=2), encoding='utf-8')
print(json.dumps(record))
sys.exit(run.returncode)

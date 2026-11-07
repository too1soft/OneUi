"""Bounded build runner for the user-approved cross-machine mobile budget."""
import json
from pathlib import Path
import shlex
import subprocess
import sys
import time


def guarded_run(command, build, audit_file, ssh_host=None):
    if audit_file is None:
        subprocess.run(command, check=True)
        return
    paths = [row['path'] for row in json.loads(Path(audit_file).read_text(encoding='utf-8-sig'))['roots']]
    auditor = Path.home() / '.codex/skills/build-artifact-hygiene/scripts/audit_artifacts.py'
    remote = "from pathlib import Path;p=Path.home()/'.cache/oneui-release-review';print(sum(x.stat().st_size for x in p.rglob('*') if x.is_file() and not x.is_symlink()))"
    def audit():
        args = [sys.executable, str(auditor), '--kind', 'build', '--max-gib', '40']
        for path in paths: args += ['--root', path]
        result = subprocess.run(args, capture_output=True, text=True, encoding='utf-8')
        data = json.loads(result.stdout)
        roots = [Path(path).resolve() for path in paths]
        for issue in data.get('incomplete', []):
            if not issue.startswith('Skipped link: '): raise RuntimeError('Incomplete artifact audit: ' + issue)
            link = Path(issue[len('Skipped link: '):])
            destination = link.resolve()
            if not any(destination == root or destination.is_relative_to(root) for root in roots):
                raise RuntimeError('Uncounted artifact link: ' + str(link))
        other = 0
        if ssh_host:
            other = int(subprocess.check_output(['ssh', '-o', 'BatchMode=yes', '-o', 'StrictHostKeyChecking=yes', ssh_host,
                '/usr/bin/python3 -c ' + shlex.quote(remote)], text=True, timeout=30))
        total = data['logical_bytes'] + other
        if total > 40 * 1024**3: raise RuntimeError('OneUI cross-machine budget exceeded: ' + str(total))
        if any(volume.get('free_bytes', 0) < 21 * 1024**3 for volume in data.get('volumes', [])):
            raise RuntimeError('Build volume headroom below limit')
        return total
    audit()
    log = Path(build) / 'current-build.log'
    with log.open('w', encoding='utf-8') as output:
        process = subprocess.Popen(command, stdout=output, stderr=subprocess.STDOUT)
        count = 0; last = time.monotonic()
        try:
            while process.poll() is None:
                time.sleep(1)
                files = sum(1 for path in Path(build).rglob('*') if path.is_file())
                if files - count >= 100 or time.monotonic() - last >= 60:
                    total = audit(); count = files; last = time.monotonic()
                    print(json.dumps({'managed_bytes': total, 'build_files': files}), flush=True)
        except BaseException:
            # Stop only this runner's compiler tree; never user/other-task processes.
            if sys.platform == 'win32': subprocess.run(['taskkill', '/PID', str(process.pid), '/T', '/F'], capture_output=True)
            else: process.terminate()
            process.wait(timeout=30)
            raise
    if process.returncode: raise RuntimeError(log.read_text(encoding='utf-8', errors='replace')[-7000:])
    audit()

#!/usr/bin/env python3
"""Verify committed OneUI plugin sources on the existing oneui-mac SSH host."""
import argparse
import base64
import json
from pathlib import Path
import re
import shlex
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ssh-host', default='oneui-mac')
    parser.add_argument('--source-commit', default='HEAD')
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--cli', type=Path, help='A cross-compiled Darwin arm64 CLI to execute on the Mac')
    parser.add_argument('--cleanup-only', action='store_true', help='Clean owned caches after a successful matching report')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    commit = subprocess.check_output(['git', '-C', str(root), 'rev-parse',
                                      args.source_commit + '^{commit}'], text=True).strip()
    if not re.fullmatch('[0-9a-f]{40}', commit) or not 1 <= args.jobs <= 8:
        raise ValueError('Expected a source commit and 1-8 build jobs')
    helper = (root / 'scripts/verify_plugins_macos_remote.py').read_bytes()
    program = 'import base64;exec(base64.b64decode(' + repr(base64.b64encode(helper).decode()) + '))'
    command = ['ssh', '-o', 'BatchMode=yes', '-o', 'StrictHostKeyChecking=yes',
               '-o', 'ConnectTimeout=10', args.ssh_host, '/usr/bin/python3 -c ' + shlex.quote(program)]
    payload = dict(commit=commit, jobs=args.jobs, cleanupOnly=args.cleanup_only,
                   componentValidator=(root / 'scripts/test-component-sdk.py').read_text(encoding='utf-8'),
                   catalogValidator=(root / 'scripts/test-plugin-catalog.py').read_text(encoding='utf-8'))
    if args.cli:
        payload['cli'] = base64.b64encode(args.cli.read_bytes()).decode()
    output = root / 'out/plugins-current/macos'
    output.mkdir(parents=True, exist_ok=True)
    with (output / 'verification.log').open('w', encoding='utf-8') as log:
        process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, text=True, encoding='utf-8', errors='replace')
        process.stdin.write(json.dumps(payload))
        process.stdin.close()
        for line in process.stdout:
            print(line, end='', flush=True)
            log.write(line)
            log.flush()
        code = process.wait()
    fetch = ['ssh', '-o', 'BatchMode=yes', '-o', 'StrictHostKeyChecking=yes',
             '-o', 'ConnectTimeout=10', args.ssh_host,
             'cat .cache/oneui-release-review/plugins-current/verification.json']
    result = subprocess.run(fetch, capture_output=True, text=True, encoding='utf-8', errors='replace')
    if result.returncode == 0:
        report = json.loads(result.stdout)
        (output / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    else:
        print('Remote report could not be fetched: ' + result.stderr.strip())
    raise SystemExit(code or result.returncode)


if __name__ == '__main__':
    main()

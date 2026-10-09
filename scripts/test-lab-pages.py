#!/usr/bin/env python3
"""Launch every catalog route in both themes and viewport sizes.

This is a startup/render smoke test, not native interaction acceptance. Reuses
one capture and one log; reports contain hashes rather than retaining 54 images.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import time


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    catalog = json.loads((root / 'examples/performance_lab/capabilities.json').read_text(encoding='utf-8'))
    args.output.mkdir(parents=True, exist_ok=True)
    capture = args.output.resolve() / 'unified-page-smoke.png'
    log = args.output / 'unified-page-smoke.log'
    results = []
    failed = False
    for group in catalog['groups']:
        for width, height, light in [(1320, 1000, False), (640, 820, True)]:
            started = time.time_ns()
            command = [str(args.exe.resolve()), '--page', group['page'], '--width', str(width),
                       '--height', str(height), '--snapshot', str(capture), '--snapshot-exit',
                       '--renderer', 'cpu']
            if light:
                command.append('--editor-light')
            entry = dict(page=group['page'], width=width, height=height, light=light, passed=False)
            with log.open('w', encoding='utf-8') as runtime:
                try:
                    process = subprocess.run(command, stdout=runtime, stderr=subprocess.STDOUT, timeout=25)
                    entry['exit_code'] = process.returncode
                    if process.returncode:
                        raise RuntimeError('Demo exited with an error')
                    if not capture.is_file() or capture.stat().st_mtime_ns < started - 2_000_000_000:
                        raise RuntimeError('A fresh capture was not produced')
                    data = capture.read_bytes()
                    if data[:8] != b'\x89PNG\r\n\x1a\n' or len(data) < 512:
                        raise RuntimeError('Capture is not a valid nonempty PNG')
                    entry['physical_size'] = struct.unpack('>II', data[16:24])
                    entry['sha256'] = hashlib.sha256(data).hexdigest()
                    entry['passed'] = True
                except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
                    entry['error'] = str(error)
                    failed = True
            if not entry['passed']:
                entry['diagnostic'] = log.read_text(encoding='utf-8', errors='replace')[-3000:]
            results.append(entry)
            print(group['page'], 'light/narrow' if light else 'dark/wide',
                  'PASS' if entry['passed'] else 'FAIL', flush=True)
            (args.output / 'unified-page-smoke.json').write_text(
                json.dumps(dict(passed=not failed, scope='startup and capture only; CPU renderer',
                                completed=len(results), expected=len(catalog['groups']) * 2,
                                results=results), indent=2) + '\n', encoding='utf-8')
            if failed:
                return 1  # Preserve the single failing log for diagnosis.
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

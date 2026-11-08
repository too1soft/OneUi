"""Compatibility entry point for native UUID release verification."""
import argparse
import json
from pathlib import Path
import subprocess
import sys


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--public', action='store_true')
    parser.add_argument('--cli', type=Path, default=root / 'dist/oneui-tools/0.1.0-preview.6/oneui-site-0.1.0-preview.6-windows-amd64.exe')
    parser.add_argument('--source', type=Path, default=root)
    parser.add_argument('--work', type=Path, default=root / 'out/plugins-current/uuid-release')
    args = parser.parse_args()
    version = json.loads((args.source / 'plugins/uuid/oneui-plugin.json').read_text(encoding='utf-8'))['version']
    command = [sys.executable, '-B', str(root / 'scripts/test-uuid-release.py'), '--source', str(args.source),
               '--cli', str(args.cli), '--work', str(args.work), '--output', str(root / 'dist/oneui-plugins' / version)]
    if args.public: command.append('--public')
    raise SystemExit(subprocess.run(command).returncode)


if __name__ == '__main__':
    main()

"""Release CLI: explicit paths only, no workspace DLL discovery."""
import argparse
import json
import sys
from pathlib import Path
from compatlib import ContractError, inventory, load, validate_manifest, verify_lock


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    check = commands.add_parser('verify-lock')
    check.add_argument('--lock', required=True)
    check.add_argument('--root', default=str(Path(__file__).parent))
    check.add_argument('--input', action='append', default=[], help='locked-name=absolute-input-root')
    check.add_argument('--only', action='append', default=[], help='verify selected inputs plus the complete foundation lock')
    manifest = commands.add_parser('verify-manifest')
    manifest.add_argument('--root', required=True)
    manifest.add_argument('--release', action='store_true')
    manifest.add_argument('--evidence-root')
    manifest.add_argument('--exceptions', help='separately approved, exact-build native x64 deferrals')
    listing = commands.add_parser('inventory')
    listing.add_argument('--root', required=True)
    args = parser.parse_args()
    try:
        if args.command == 'verify-lock':
            roots = {}
            for item in args.input:
                key, separator, path = item.partition('=')
                if not separator or key in roots or not Path(path).is_absolute():
                    raise ContractError('inputs must be unique name=absolute-path mappings')
                roots[key] = path
            lock = load(args.lock)
            if args.only:
                if any(key not in lock['inputs'] for key in args.only):
                    raise ContractError('required input is absent from the project lock')
                lock['inputs'] = {key: lock['inputs'][key] for key in args.only}
            verify_lock(lock, args.root, roots)
        elif args.command == 'verify-manifest':
            validate_manifest(load(Path(args.root) / 'build-manifest.json'), args.root, args.release, args.evidence_root,
                              load(args.exceptions) if args.exceptions else None)
        else:
            print(json.dumps(inventory(args.root), indent=2))
            return 0
        print('PASS: ' + args.command)
        return 0
    except (ContractError, OSError, KeyError, TypeError, ValueError) as error:
        print('BLOCKED: ' + str(error), file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())

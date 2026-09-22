"""Compatibility entry point; audit implementation lives in windows-compat."""
import sys
from windows_compat_loader import load_shared
_shared = load_shared('audit_windows.py')
audit = _shared.audit
make_reference = _shared.make_reference
resolve_export = _shared.resolve_export
main = _shared.main
if __name__ == '__main__':
    sys.exit(main())

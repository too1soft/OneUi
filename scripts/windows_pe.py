"""Compatibility shim; PE implementation is maintained in windows-compat."""
from windows_compat_loader import load_shared
_shared = load_shared('windows_pe.py')
PE = _shared.PE
PEError = _shared.PEError

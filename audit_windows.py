"""Audit against exports from an actual, architecture-matched OS baseline.

No DLL-prefix allowlist, no loading untrusted DLLs, no new API silently accepted
because its module is kernel32.dll. Reference JSON contains metadata only, not
redistributed Windows binaries. Export audit is NOT a substitute for VM tests.
"""
import argparse
import json
from pathlib import Path
import sys

from windows_pe import PE, PEError


def resolve_export(modules, module, symbol, seen=None):
    seen = set() if seen is None else seen
    key = (module, symbol)
    if key in seen or len(seen) > 32:
        return False
    seen.add(key)
    exports = modules.get(module, {}).get("exports", {})
    if symbol not in exports:
        return False
    forward = exports[symbol]
    if forward is None:
        return True
    name, separator, target = forward.rpartition(".")
    if not separator:
        return False
    name = name.lower()
    if not name.endswith(".dll"):
        name += ".dll"
    return resolve_export(modules, name, target, seen)


def audit(paths, reference, runtime_dirs=()):
    modules = reference["modules"]
    expected_arch = reference["architecture"]
    pending = list(map(Path, paths))
    visited, results, errors = set(), [], []
    while pending:
        path = pending.pop().resolve()
        if path in visited:
            continue
        visited.add(path)
        try:
            pe = PE(path)
            if pe.arch != expected_arch:
                raise PEError(f"architecture {pe.arch} != reference {expected_arch}")
            if pe.subsystem_version > tuple(reference["maximumSubsystemVersion"]):
                raise PEError(f"subsystem version {pe.subsystem_version} exceeds OS baseline")
            imports = list(pe.imports())
            for entry in imports:
                module, symbol = entry["module"], entry["symbol"]
                # Never approve an app-local replacement for a baseline OS DLL.
                if module in modules:
                    if not resolve_export(modules, module, symbol):
                        errors.append(f"{path.name}: {entry['kind']} import {module}!{symbol} absent/unresolved on baseline")
                    continue
                # Runtime/API-set DLLs missing on baseline cannot be supplied
                # as an undocumented external prerequisite.
                if module.startswith(("api-ms-", "ext-ms-", "vcruntime", "msvcp", "ucrtbase")):
                    errors.append(f"{path.name}: unsupported runtime/API-set {module}")
                    continue
                candidates = [directory / module for directory in [path.parent, *map(Path, runtime_dirs)]]
                dependency = next((item for item in candidates if item.is_file()), None)
                if dependency is None:
                    errors.append(f"{path.name}: missing non-system dependency {module}")
                    continue
                dependency_pe = PE(dependency)
                exports = dependency_pe.exports()
                if symbol not in exports:
                    errors.append(f"{path.name}: bundled {module} does not export {symbol}")
                elif exports[symbol] is not None:
                    errors.append(f"{path.name}: bundled export forwarder requires explicit review: {module}!{symbol}")
                pending.append(dependency)
            results.append({"path": str(path), "architecture": pe.arch, "sha256": pe.sha256(), "imports": imports})
        except (OSError, PEError) as exc:
            errors.append(f"{path}: {exc}")
    return {"passed": not errors, "reference": reference["label"], "architecture": expected_arch,
            "runtimeTested": False, "binaries": results, "errors": errors}


def make_reference(directory, label, architecture):
    modules, excluded = {}, {}
    for path in sorted(p for p in Path(directory).iterdir() if p.suffix.lower() in (".dll", ".drv")):
        try:
            pe = PE(path)
        except PEError as exc:
            # System32 also contains 16-bit NE drivers. They are NOT approved
            # native dependencies; retain the exclusion in the reference.
            excluded[path.name.lower()] = str(exc)
            continue
        if pe.arch != architecture:
            raise PEError(f"reference architecture mismatch: {path}")
        modules[path.name.lower()] = {"sha256": pe.sha256(), "exports": pe.exports()}
    if not {"kernel32.dll", "kernelbase.dll", "ntdll.dll", "user32.dll"} <= modules.keys():
        raise PEError("incomplete OS reference: kernel32, kernelbase, ntdll and user32 are required")
    if "GetSystemTimePreciseAsFileTime" in modules["kernel32.dll"]["exports"]:
        raise PEError("reference contains Windows 8+ time API; not a Windows 7 baseline")
    return {"schemaVersion": 1, "label": label, "architecture": architecture,
            "maximumSubsystemVersion": [6, 1], "modules": modules, "excludedModules": excluded}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", action="append", default=[])
    parser.add_argument("--directory", help="Audit all EXE/DLL files recursively")
    parser.add_argument("--runtime-dir", action="append", default=[])
    parser.add_argument("--reference")
    parser.add_argument("--output")
    parser.add_argument("--capture-reference", help="Directory of pristine Win7 SP1 System32 DLLs")
    parser.add_argument("--label")
    parser.add_argument("--architecture", choices=["x86", "x64"])
    args = parser.parse_args()
    try:
        if args.capture_reference:
            if not all((args.output, args.label, args.architecture)):
                parser.error("capture requires --output, --label and --architecture")
            result = make_reference(args.capture_reference, args.label, args.architecture)
            Path(args.output).write_text(json.dumps(result, ensure_ascii=False, sort_keys=True, indent=2) + "\n", encoding="utf-8")
            print(f"Captured {len(result['modules'])} OS modules ({args.architecture}); metadata only")
            return 0
        if not args.reference:
            parser.error("--reference is required; host Windows exports are not a Win7 baseline")
        paths = list(map(Path, args.binary))
        if args.directory:
            paths.extend(p for p in Path(args.directory).rglob("*") if p.suffix.lower() in (".exe", ".dll"))
        if not paths:
            parser.error("no binaries to audit")
        reference = json.loads(Path(args.reference).read_text(encoding="utf-8"))
        result = audit(paths, reference, args.runtime_dir)
        if args.output:
            Path(args.output).write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(json.dumps({k: v for k, v in result.items() if k != "binaries"}, ensure_ascii=False, indent=2))
        print(f"Inspected {len(result['binaries'])} binaries; normal and delay imports, ordinal exports, forwarders")
        return 0 if result["passed"] else 1
    except (OSError, ValueError, KeyError) as exc:
        print(f"Audit failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())

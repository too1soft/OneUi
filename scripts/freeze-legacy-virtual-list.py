"""Freeze fixtures using the two real historical viewc binaries, never current viewc.

Run only with the integration build slot. Outputs replace fixed source fixtures;
no build/configure is performed. The complete COMP payload is checked against
independent frozen wire bytes before the fixtures receive provenance records.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess


def u32(value):
    return struct.pack("<I", value)


def text(value):
    value = value.encode("utf-8")
    return u32(len(value)) + value


def expected_comp(branch):
    properties = [
        (1, "basis", 3, 1, ""), (2, "class", 4, 1, ""),
        (3, "description", 4, 3, ""), (4, "disabled", 1, 3, ""),
        (5, "grow", 3, 1, ""), (6, "items", 5, 2, "@list.item"),
        (7, "max", 3, 1, ""), (8, "min", 3, 1, ""),
        (9, "name", 4, 3, ""), (10, "selectedIndex", 2, 7, ""),
        (11, "shrink", 3, 1, ""), (12, "visible", 1, 3, ""),
        (13, "row-height", 3, 3, ""), (14, "scroll-offset", 3, 3, ""),
    ]
    events = [(1, "activate", [])]
    if branch == "Terminal":
        properties += [(15, "multiple", 1, 3, "")]
        events += [(2, "selectionChanged", [("index", 2), ("selected", 1)])]
    else:
        properties += [(15, "trailing-width", 3, 3, ""),
                       (16, "trailing-gap", 3, 3, ""),
                       (17, "activate-on-single-click", 1, 3, "")]
    data = b"OVCOMP1\0" + u32(1) + text("VirtualList")
    data += u32(2) + u32(len(properties)) + u32(len(events))
    for pid, name, tag, flags, record in properties:
        data += u32(pid) + text(name) + u32(tag) + u32(flags) + text(record)
    for eid, name, args in events:
        data += u32(eid) + text(name) + u32(len(args))
        for name, tag in args:
            data += text(name) + u32(tag)
    return data + text("leaf") + u32(10)


def component_section(data):
    if data[:8] != b"ONEVIEW\0":
        raise ValueError("not an actual ONEVIEW image")
    directory, count = struct.unpack_from("<QI", data, 24)
    for at in range(directory, directory + count * 32, 32):
        kind, _, offset, length, _, _ = struct.unpack_from("<IIQQII", data, at)
        if kind == 3:
            return data[offset:offset + length]
    raise ValueError("missing COMP")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--terminal-viewc", required=True, type=Path)
    parser.add_argument("--tools-viewc", required=True, type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    fixtures = root / "tests/views/legacy"
    manifest = {"format": 1, "purpose": "actual pre-integration VirtualList v2 IR", "compilers": {}}
    for branch, executable in (("Terminal", args.terminal_viewc), ("Tools", args.tools_viewc)):
        executable = executable.resolve(strict=True)
        if root in executable.parents:
            raise ValueError("the integration compiler cannot generate historical fixtures")
        record = {"executable": str(executable), "sha256": digest(executable), "fixtures": {}}
        for variant in ("Static", "Bound", "Call", "Reset", "ResetCall"):
            source = fixtures / (f"{variant}.one" if variant.startswith("Reset") else f"{branch}{variant}.one")
            output = fixtures / f"{branch}{variant}.oneview"
            subprocess.run([str(executable), "--emit", "ir", "--input", str(source),
                            "--schema", str(fixtures / "VirtualList.oneschema"),
                            "--name", "LegacyVirtualList", "--output", str(output)], check=True)
            comp = component_section(output.read_bytes())
            if comp != expected_comp(branch):
                raise ValueError(f"{output.name}: compiler does not emit the exact frozen {branch} v2 contract")
            record["fixtures"][output.name] = {"sha256": digest(output), "source_sha256": digest(source),
                                                "comp_sha256": hashlib.sha256(comp).hexdigest()}
        manifest["compilers"][branch] = record
    for source in ("VirtualList.oneschema", "ListRoot.one"):
        manifest[source] = digest(fixtures / source)
    (fixtures / "provenance.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print("Frozen 10 actual old-compiler IR fixtures with exact COMP and SHA256 provenance.")


if __name__ == "__main__":
    main()

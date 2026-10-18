#!/usr/bin/env python3
"""Freeze real old compiler output; never synthesize COMP or downgrade an IR.

Run only while holding the shared compiler/output slot, before rebuilding the
old executable. The expected SHA must be obtained independently from that EXE.
Descriptors in Baseline.contracts.json were extracted from git64a5b89 source;
every historical field is compared with the actual compiler output here.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess


def digest(data):
    return hashlib.sha256(data).hexdigest()


def contracts(data):
    if len(data) < 128 or data[:8] != b"ONEVIEW\0":
        raise ValueError("unsupported ONEVIEW header")
    major, minor, header, size, directory = struct.unpack_from("<HHIQQ", data, 8)
    if major != 1 or minor > 1 or header < 128 or size != len(data) or directory != header:
        raise ValueError("unsupported ONEVIEW format or directory")
    if any(data[128:header]):
        raise ValueError("unsupported header extension")
    count = struct.unpack_from("<I", data, 32)[0]
    if count > 1024 or directory > len(data) or count * 32 > len(data) - directory:
        raise ValueError("invalid section directory range")
    comp = None
    seen = set()
    ranges = []
    for i in range(count):
        kind, flags, offset, length, items, stride = struct.unpack_from("<IIQQII", data, directory + i * 32)
        if kind in seen or flags > 1 or (kind > 19 and flags):
            raise ValueError("duplicate or unsupported required section")
        seen.add(kind)
        if offset % 8 or offset < directory + count * 32 or offset > len(data) or length > len(data) - offset:
            raise ValueError("invalid section boundary")
        ranges.append((offset, length))
        if kind == 3:
            if flags != 1 or stride != 0:
                raise ValueError("unsupported COMP flags or stride")
            comp = (offset, length, items)
    end = directory + count * 32
    for offset, length in sorted(ranges):
        if offset < end or any(data[end:offset]):
            raise ValueError("overlapping sections or nonzero padding")
        end = offset + length
    if end != len(data):
        raise ValueError("undeclared trailing file data")
    if comp is None:
        raise ValueError("missing COMP section")
    offset, length, items = comp
    blob = data[offset:offset + length]
    if len(blob) < 12 or blob[:8] != b"OVCOMP1\0" or hashlib.sha256(blob).digest() != data[80:112]:
        raise ValueError("unsupported or corrupt COMP section")
    pos = 8

    def number():
        nonlocal pos
        if pos + 4 > len(blob):
            raise ValueError("truncated COMP integer")
        value = struct.unpack_from("<I", blob, pos)[0]
        pos += 4
        return value

    def string():
        nonlocal pos
        size = number()
        if size > 1024 * 1024 or pos + size > len(blob):
            raise ValueError("invalid COMP string boundary")
        value = blob[pos:pos + size].decode("utf-8")
        pos += size
        return value

    result = []
    actual_count = number()
    if actual_count != items or actual_count > 100000:
        raise ValueError("invalid COMP descriptor count")
    for _ in range(actual_count):
        item = {"name": string(), "version": number()}
        properties, events = number(), number()
        item["properties"] = [
            {"id": number(), "name": string(), "tag": number(),
             "flags": number(), "record": string()}
            for _ in range(properties)
        ]
        item["events"] = []
        for _ in range(events):
            event = {"id": number(), "name": string()}
            event["args"] = [{"name": string(), "tag": number()} for _ in range(number())]
            item["events"].append(event)
        item["child_rule"] = string()
        item["model_property"] = number()
        result.append(item)
    if pos != len(blob):
        raise ValueError("unexpected COMP trailing bytes")
    return result, digest(blob)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--expected-compiler-sha256", required=True)
    args = parser.parse_args()
    compiler = args.compiler.resolve(strict=True)
    compiler_hash = digest(compiler.read_bytes())
    if compiler_hash != args.expected_compiler_sha256.lower():
        raise ValueError("old compiler SHA mismatch; do not substitute a new compiler")
    root = Path(__file__).resolve().parents[1]
    directory = root / "tests/views/legacy"
    expected_path = directory / "Baseline.contracts.json"
    expected = json.loads(expected_path.read_text(encoding="utf-8"))
    schema = directory / "Baseline.oneschema"
    provenance = {
        "format": 1,
        "purpose": "Real pre-integration baseline IR, verified against exact git source descriptors",
        "compiler": str(compiler),
        "compiler_sha256": compiler_hash,
        "baseline_commit": expected["baseline_commit"],
        "expected_descriptors_sha256": digest(expected_path.read_bytes()),
        "schema_sha256": digest(schema.read_bytes()),
        "fixtures": {},
    }
    temporary = directory / "Baseline.freeze.oneview"
    if temporary.exists():
        raise ValueError("reserved freeze output already exists; inspect it before continuing")
    try:
        for contract in expected["contracts"]:
            stem = "Baseline" + contract["name"]
            source = directory / (stem + ".one")
            destination = directory / (stem + ".oneview")
            command = [str(compiler), "--emit", "ir", "--input", str(source),
                       "--schema", str(schema), "--name", stem, "--output", str(temporary)]
            subprocess.run(command, check=True, cwd=root)
            data = temporary.read_bytes()
            actual, comp_hash = contracts(data)
            matches = [item for item in actual if item["name"] == contract["name"]]
            if matches != [contract]:
                raise ValueError(f"real compiler contract differs from baseline: {contract['name']}")
            if destination.exists() and destination.read_bytes() != data:
                raise ValueError(f"existing frozen asset differs: {destination}")
            destination.write_bytes(data)
            if destination.read_bytes() != data:
                raise ValueError("freeze byte verification failed")
            provenance["fixtures"][destination.name] = {
                "sha256": digest(data), "size_bytes": len(data), "comp_sha256": comp_hash,
                "source": str(source), "source_sha256": digest(source.read_bytes()),
                "command": command, "contracts": actual,
            }
            temporary.unlink()
        if digest(compiler.read_bytes()) != compiler_hash:
            raise ValueError("compiler changed during freeze")
        output = directory / "Baseline.provenance.json"
        output.write_text(json.dumps(provenance, indent=2) + "\n", encoding="utf-8")
        print(f"Verified and froze {len(provenance['fixtures'])} real baseline inputs: {output}")
    finally:
        if temporary.exists():
            temporary.unlink()


if __name__ == "__main__":
    main()

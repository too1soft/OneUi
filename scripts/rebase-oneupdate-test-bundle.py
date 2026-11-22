"""Replace only the executable stub of an existing OneUpdate V3 QA bundle.

The signed plan, package, agent, logo and trailer remain byte-for-byte intact.
This does not sign a bundle or change its trust policy. Outputs are TEST files.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

MAGIC = b"ONEUPDATEBUNDLE3"
TRAILER_BYTES = 48


def split_bundle(data):
    if len(data) < TRAILER_BYTES or data[-TRAILER_BYTES:-32] != MAGIC:
        raise ValueError("Expected an unsigned OneUpdate V3 QA bundle")
    sizes = struct.unpack_from("<QQQQ", data, len(data) - 32)
    start = len(data) - TRAILER_BYTES - sum(sizes)
    if start < 2 or data[:2] != b"MZ":
        raise ValueError("Invalid bundle lengths or executable")
    # Retain the complete config/payload/agent/logo/trailer region.
    return start, sizes


def rebase(source, stub):
    start, sizes = split_bundle(source)
    if stub[:2] != b"MZ" or stub[-TRAILER_BYTES:-32] == MAGIC:
        raise ValueError("Expected a standalone Windows installer stub")
    result = stub + source[start:]
    result_start, result_sizes = split_bundle(result)
    if result_sizes != sizes or result[result_start:] != source[start:]:
        raise ValueError("Bundle content changed")
    return result, {"source_stub_bytes": start, "stub_bytes": len(stub),
                    "bundle_bytes": len(result), "retained_tail_bytes": len(source) - start,
                    "retained_tail_sha256": hashlib.sha256(source[start:]).hexdigest(),
                    "stub_sha256": hashlib.sha256(stub).hexdigest(),
                    "bundle_sha256": hashlib.sha256(result).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--stub", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    if "TEST" not in args.output.stem.upper() or args.output.suffix.lower() != ".exe":
        raise ValueError("Output filename must contain TEST and end in .exe")
    if args.output.exists() or args.report.exists():
        raise ValueError("Refusing to overwrite an existing comparison")
    output, report = rebase(args.source.read_bytes(), args.stub.read_bytes())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(output)
    args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report))


if __name__ == "__main__":
    main()

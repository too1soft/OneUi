"""Replace only the executable stub of an existing OneUpdate V3 QA bundle.

By default, the entire configuration/payload/agent/logo/trailer tail stays exact.
The explicit acceptance-only option selects the existing admin consent policy
for isolated lifecycle tests; signed plans and payloads stay unchanged. Neither
mode signs a bundle or bypasses verification. Outputs are TEST files.
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
    config = json.loads(data[start:start + sizes[0]])
    if config.get("installed_shell"):
        raise ValueError("Installed-shell offsets/deltas require Worker reassembly; cannot rebase this bundle")
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


def acceptance_policy(source):
    """Select the product's supported admin policy only for isolated QA."""
    start, sizes = split_bundle(source)
    config = json.loads(source[start:start + sizes[0]])
    config["product_spec"]["legal"]["silent_acceptance"] = "admin_policy"
    encoded = json.dumps(config, ensure_ascii=False, separators=(",", ":")).encode()
    unchanged = source[start + sizes[0]:-TRAILER_BYTES]
    return (source[:start] + encoded + unchanged + MAGIC +
            struct.pack("<QQQQ", len(encoded), *sizes[1:]))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--stub", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--acceptance-admin-policy", action="store_true",
                        help="Isolated lifecycle fixture only; original comparison bundles retain their policy")
    args = parser.parse_args()
    if "TEST" not in args.output.stem.upper() or args.output.suffix.lower() != ".exe":
        raise ValueError("Output filename must contain TEST and end in .exe")
    if args.output.exists() or args.report.exists():
        raise ValueError("Refusing to overwrite an existing comparison")
    source = args.source.read_bytes()
    if args.acceptance_admin_policy:
        source = acceptance_policy(source)
    output, report = rebase(source, args.stub.read_bytes())
    if args.acceptance_admin_policy:
        report["acceptance_only_configuration_change"] = "product_spec.legal.silent_acceptance=admin_policy"
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(output)
    args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report))


if __name__ == "__main__":
    main()

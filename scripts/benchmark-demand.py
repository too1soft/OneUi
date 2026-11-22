"""Measure one executable repeatedly, recording native pixels and PE exports."""
import argparse
import hashlib
import json
import statistics
import struct
import subprocess
from pathlib import Path


def exports(data):
    pe = struct.unpack_from("<I", data, 60)[0]
    count, optional_size = struct.unpack_from("<H12xH", data, pe + 6)
    optional = pe + 24
    magic = struct.unpack_from("<H", data, optional)[0]
    address = struct.unpack_from("<I", data, optional + (112 if magic == 523 else 96))[0]
    for index in range(count):
        offset = optional + optional_size + index * 40
        virtual_size, virtual, raw_size, raw = struct.unpack_from("<IIII", data, offset + 8)
        if virtual <= address < virtual + max(virtual_size, raw_size):
            return struct.unpack_from("<I", data, raw + address - virtual + 24)[0]
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--runs", type=int, default=9)
    args = parser.parse_args()
    samples = []
    pixels = args.output.with_suffix(".bgra")
    for _ in range(args.runs):
        result = subprocess.run([str(args.executable.resolve()), str(pixels.resolve())],
                                check=True, capture_output=True, text=True, encoding="utf-8")
        samples.append(json.loads(result.stdout))
    data = args.executable.read_bytes()
    report = {
        "executable_bytes": len(data), "named_exports": exports(data),
        "executable_sha256": hashlib.sha256(data).hexdigest(),
        "pixels_sha256": hashlib.sha256(pixels.read_bytes()).hexdigest(),
        "runs": args.runs, "samples": samples,
        "median": {key: statistics.median(sample[key] for sample in samples)
                   for key in samples[0]},
    }
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({key: value for key, value in report.items() if key != "samples"}))


if __name__ == "__main__":
    main()

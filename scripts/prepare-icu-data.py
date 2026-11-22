"""Repackage pinned ICU data while preserving every retained entry byte-for-byte.

The text profile keeps all break rules/dictionaries/locales, normalization,
Unicode properties and root locale resources. It omits non-UI ICU services
(collation, legacy charset conversion, display names, currencies and zones).
"""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path


def parse(data):
    if len(data) < 32 or data[2:4] != b"\xda\x27" or data[12:16] != b"CmnD":
        raise ValueError("Expected an ICU common-data package")
    if data[8:11] != b"\x00\x00\x02" or data[16] != 1:
        raise ValueError("Unsupported ICU byte order, character width or package version")
    header = struct.unpack_from("<H", data)[0]
    if header < 24 or header % 16 or header + 4 > len(data):
        raise ValueError("Invalid ICU header")
    count = struct.unpack_from("<I", data, header)[0]
    names_start = 4 + count * 8
    if not count or header + names_start > len(data):
        raise ValueError("Invalid ICU table of contents")
    entries = []
    for index in range(count):
        name, offset = struct.unpack_from("<II", data, header + 4 + index * 8)
        if name < names_start or offset % 16 or header + offset >= len(data):
            raise ValueError("Invalid ICU entry offset")
        end = data.find(b"\0", header + name, header + offset)
        if end < 0:
            raise ValueError("Invalid ICU entry name")
        text = data[header + name:end].decode("ascii")
        if "/" not in text or any(part in ("", ".", "..") for part in text.split("/")):
            raise ValueError("Unsafe ICU entry name")
        entries.append((text, offset))
    if [name for name, _ in entries] != sorted({name for name, _ in entries}):
        raise ValueError("ICU entry names must be unique and sorted")
    offsets = sorted({offset for _, offset in entries})
    if len(offsets) != count or offsets[0] <= max(name for name, _ in
            (struct.unpack_from("<II", data, header + 4 + i * 8) for i in range(count))):
        raise ValueError("Overlapping ICU entries")
    ends = dict(zip(offsets, offsets[1:] + [len(data) - header]))
    return data[:header], [(name, data[header + offset:header + ends[offset]])
                           for name, offset in entries]


def keep_text(name):
    relative = name.split("/", 1)[1]
    if relative.startswith("brkitr/"):
        return True
    if "/" in relative:
        return False
    if relative.endswith((".icu", ".nrm")):
        return True
    # Per-locale top-level bundles contain calendar/number formatting, not
    # break-iterator tailoring (which lives under brkitr/ and stays intact).
    # Keep root, the pooled strings, locale index and every global service
    # resource; locale canonicalization can still resolve all known names.
    return relative.endswith(".res") and (relative == "res_index.res" or
        not re.fullmatch(r"[a-z]{2,3}(?:_[A-Za-z0-9]+)*\.res", relative))


def prepare(data, profile):
    header, entries = parse(data)
    if profile == "full":
        return data, entries
    if profile != "text":
        raise ValueError("Unknown ICU profile")
    selected = [(name, blob) for name, blob in entries if keep_text(name)]
    if not any(name.endswith("/brkitr/cjdict.dict") for name, _ in selected):
        raise ValueError("CJK dictionary missing from pinned ICU input")
    body = bytearray(4 + len(selected) * 8)
    struct.pack_into("<I", body, 0, len(selected))
    names = []
    for name, _ in selected:
        names.append(len(body))
        body.extend(name.encode("ascii") + b"\0")
    body.extend(b"\0" * (-len(body) % 16))
    for index, ((_, blob), name_offset) in enumerate(zip(selected, names)):
        struct.pack_into("<II", body, 4 + index * 8, name_offset, len(body))
        body.extend(blob)
        body.extend(b"\0" * (-len(body) % 16))
    result = bytes(header + body)
    _, checked = parse(result)
    if checked != selected:
        raise ValueError("Repackaged ICU entries differ from the pinned input")
    return result, selected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--profile", choices=("full", "text"), default="full")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    source = args.input.read_bytes()
    data, selected = prepare(source, args.profile)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(data)
    if args.report:
        report = {"profile": args.profile, "input_bytes": len(source), "output_bytes": len(data),
                  "input_sha256": hashlib.sha256(source).hexdigest(),
                  "output_sha256": hashlib.sha256(data).hexdigest(),
                  "entries": [{"name": name, "bytes": len(blob),
                               "sha256": hashlib.sha256(blob).hexdigest()} for name, blob in selected]}
        args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()

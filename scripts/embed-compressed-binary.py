#!/usr/bin/env python3
"""Create a deterministic C++ source file for a zlib-compressed binary."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import zlib


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--symbol", required=True)
    args = parser.parse_args()

    source = Path(args.input).read_bytes()
    compressed = zlib.compress(source, level=9)
    digest = hashlib.sha256(source).digest()
    symbol = args.symbol

    def bytes_literal(data: bytes) -> str:
        rows = []
        for offset in range(0, len(data), 16):
            rows.append("    " + ", ".join(f"0x{value:02x}" for value in data[offset : offset + 16]) + ",")
        return "\n".join(rows)

    output = (
        "#include <cstddef>\n\n"
        f"alignas(16) extern const unsigned char {symbol}Compressed[] = {{\n"
        f"{bytes_literal(compressed)}\n"
        "};\n"
        f"extern const std::size_t {symbol}CompressedSize = sizeof({symbol}Compressed);\n"
        f"extern const std::size_t {symbol}OriginalSize = {len(source)}u;\n"
        f"extern const unsigned char {symbol}Sha256[32] = {{\n"
        f"{bytes_literal(digest)}\n"
        "};\n"
    )
    destination = Path(args.output)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(output, encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()

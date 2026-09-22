"""Normalize only the probed /showIncludes prefix for GN/Ninja.

VSLANG=1033 is insufficient when the compiler has no English language pack.
Without this wrapper Ninja records zero header dependencies on that machine.
Compiler arguments, diagnostics, header paths and exit status are preserved.
"""
import argparse
import base64
import json
from pathlib import Path
import re
import subprocess
import sys


def normalized(line, prefix):
    return b"Note: including file:" + line[len(prefix):] if line.startswith(prefix) else line


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe")
    parser.add_argument("--prefix-file", required=True)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if args.probe:
        fixture = Path(__file__).resolve().parents[1] / "tests/fixtures/showincludes_probe.cpp"
        result = subprocess.run([args.probe, "/nologo", "/EP", "/showIncludes", "/X", str(fixture)],
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if result.returncode:
            sys.stdout.buffer.write(result.stdout)
            return result.returncode
        for line in result.stdout.splitlines():
            if line.startswith(b"#") or b"showincludes_probe.h" not in line:
                continue
            match = re.search(rb"[A-Za-z]:[\\/]", line)
            if match:
                prefix = line[:match.start()].rstrip()
                if prefix:
                    Path(args.prefix_file).write_text(json.dumps({"compiler": args.probe,
                        "prefixBase64": base64.b64encode(prefix).decode("ascii")}), encoding="utf-8")
                    print("Detected MSVC include prefix; Ninja header tracking enabled")
                    return 0
        raise RuntimeError("Cannot detect /showIncludes prefix; refusing untracked incremental compilation")
    command = args.command[1:] if args.command[:1] == ["--"] else args.command
    if not command:
        parser.error("compiler command is required")
    prefix = base64.b64decode(json.loads(Path(args.prefix_file).read_text(encoding="utf-8"))["prefixBase64"], validate=True)
    if not prefix:
        raise RuntimeError("Empty include prefix")
    child = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    for line in child.stdout:
        sys.stdout.buffer.write(normalized(line, prefix))
    sys.stdout.buffer.flush()
    return child.wait()


if __name__ == "__main__":
    sys.exit(main())

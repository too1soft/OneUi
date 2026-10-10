"""CLI test harness. All generated files live in a scoped temporary directory."""
import hashlib
from pathlib import Path
import struct
import subprocess
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from workspace_fixture import temporary_workspace, SCRATCH

COMPILER = Path(sys.argv.pop(1)).resolve()
SCRATCH = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else SCRATCH
FIXTURES = Path(__file__).parent


def u32(data, at):
    return struct.unpack_from("<I", data, at)[0]


def directory(data):
    return {row[0]: row for row in (
        struct.unpack_from("<IIQQII", data, 128 + i * 32)
        for i in range(u32(data, 32)))}


def section(data, kind):
    _, _, at, size, _, _ = directory(data)[kind]
    return data[at:at + size]


class CompilerCase(unittest.TestCase):
    def setUp(self):
        SCRATCH.mkdir(parents=True, exist_ok=True)
        context = temporary_workspace(SCRATCH)
        self.root = Path(context.__enter__())
        self.addCleanup(context.__exit__, None, None, None)

    def write(self, name, contents):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        if isinstance(contents, (bytes, bytearray)):
            path.write_bytes(contents)
        else:
            path.write_text(contents, encoding="utf-8")
        return path

    def run_cli(self, *args, error=None):
        result = subprocess.run([str(COMPILER), *map(str, args)], capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=15)
        if error is None:
            self.assertEqual(result.returncode, 0, result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout)
            self.assertIn(error, result.stderr)
        return result

    def compile(self, one='<Text :text="title"/>', schema='model Basic { r title: utf8 }', error=None, raw=False):
        source = one if raw else '<template view-model="Basic">' + one + '</template>'
        self.write("Basic.one", source)
        self.write("Basic.oneschema", schema)
        output = self.root / "Basic.oneview"
        result = self.run_cli("--input", self.root / "Basic.one", "--schema", self.root / "Basic.oneschema", "--output", output, "--name", "Basic", "--emit", "ir", "--depfile", self.root / "Basic.d", error=error)
        if error is not None:
            self.assertRegex(result.stderr, r":\d+:\d+:")
            return result
        data = output.read_bytes()
        self.run_cli("--verify", output)
        self.assertEqual(data[48:80], hashlib.sha256(section(data, 2)).digest())
        self.assertEqual(data[80:112], hashlib.sha256(section(data, 3)).digest())
        return data

    def verify_bytes(self, data, error=""):
        self.run_cli("--verify", self.write("mutated.oneview", data), error=error)

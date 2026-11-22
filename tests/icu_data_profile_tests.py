"""Check retained ICU resources and compare native services in isolated processes."""
import argparse
import importlib.util
from pathlib import Path
import subprocess
import unittest

spec = importlib.util.spec_from_file_location(
    "icu_data", Path(__file__).resolve().parents[1] / "scripts/prepare-icu-data.py")
icu = importlib.util.module_from_spec(spec)
spec.loader.exec_module(icu)


class PackageTests(unittest.TestCase):
    def test_full_is_identical(self):
        self.assertEqual(icu.prepare(self.source, "full")[0], self.source)

    def test_every_text_resource_is_exact(self):
        _, original = icu.parse(self.source)
        result, retained = icu.prepare(self.source, "text")
        self.assertEqual(icu.parse(result)[1], retained)
        source = dict(original)
        self.assertTrue(retained)
        for name, blob in retained:
            self.assertEqual(source[name], blob, name)
        for name, blob in original:
            if "/brkitr/" in name:
                self.assertEqual(dict(retained)[name], blob, name)
        self.assertLess(len(result), len(self.source))

    def test_invalid_headers_fail(self):
        for data in (b"", self.source[:30], b"x" * len(self.source)):
            with self.assertRaises(ValueError):
                icu.parse(data)
        invalid = bytearray(self.source)
        invalid[8] = 1
        with self.assertRaises(ValueError):
            icu.parse(invalid)

    def test_native_locale_services_match(self):
        result, _ = icu.prepare(self.source, "text")
        self.output.mkdir(parents=True, exist_ok=True)
        candidate = self.output / "icu-text.dat"
        candidate.write_bytes(result)
        if self.native:
            full = subprocess.check_output([str(self.native), str(self.input)], text=True)
            text = subprocess.check_output([str(self.native), str(candidate)], text=True)
            self.assertEqual(full, text)
            print(full.strip())


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--native", type=Path)
    args = parser.parse_args()
    PackageTests.input, PackageTests.output, PackageTests.native = args.input, args.output, args.native
    PackageTests.source = args.input.read_bytes()
    unittest.main(argv=[__file__])

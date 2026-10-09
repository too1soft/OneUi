"""Regress immutable release history and candidate replacement semantics."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("catalog", Path(__file__).with_name("make-release-catalog.py"))
catalog = importlib.util.module_from_spec(spec)
spec.loader.exec_module(catalog)

class ReleaseHistory(unittest.TestCase):
    def test_new_version_preserves_old_release_and_channel(self):
        old = dict(version="0.1.0", channel="stable", artifacts=[{"file":"old.zip"}])
        new = dict(version="0.2.0-preview.1", channel="preview", artifacts=[{"file":"new.zip"}])
        source = dict(schemaVersion=1, releases=[old])
        self.assertEqual(catalog.merge_release(source, new)["releases"], [new, old])
        self.assertEqual(source["releases"], [old])
        self.assertEqual(catalog.merge_release(source, old), source)

    def test_existing_version_is_immutable(self):
        previous = dict(version="0.1.0-preview.1", channel="preview", artifacts=[{"file":"old.zip"}])
        changed = dict(previous, artifacts=[{"file":"changed.zip"}])
        source = dict(schemaVersion=1, releases=[previous])
        with self.assertRaises(ValueError):
            catalog.merge_release(source, changed)
        self.assertEqual(catalog.merge_release(source, changed, True)["releases"], [changed])
        previous["channel"] = "stable"
        with self.assertRaises(ValueError):
            catalog.merge_release(source, changed, True)

if __name__ == "__main__":
    unittest.main()

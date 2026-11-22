"""Exercise compiler reachability and conservative dynamic-runtime fallbacks."""
import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

COMPILER = Path(sys.argv.pop(1)).resolve()
BUILD = Path(sys.argv.pop(1)).resolve()
ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("merge_requirements", ROOT / "scripts/merge-view-requirements.py")
merger = importlib.util.module_from_spec(spec)
spec.loader.exec_module(merger)


class RequirementsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(dir=BUILD)
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "Root.oneschema").write_text(
            "model Root { r visible: bool r title: utf8 }", encoding="utf-8")

    def compile(self, body, name="Page", emit="ir"):
        source = self.root / (name + ".one")
        source.write_text(body, encoding="utf-8")
        output = self.root / (name + ".oneview")
        manifest = self.root / (name + ".json")
        args = [str(COMPILER), "--input", str(source), "--schema", str(self.root / "Root.oneschema"),
                "--output", str(output), "--emit", emit, "--requirements", str(manifest)]
        subprocess.run(args, check=True, capture_output=True)
        return manifest, output

    def test_imports_and_conditional_branches_are_preserved(self):
        (self.root / "Part.one").write_text(
            '<template view-model="Root"><CodeEditor :text="title"/></template>', encoding="utf-8")
        manifest, _ = self.compile(
            '<import name="Part" src="Part.one"/>'
            '<template view-model="Root"><Column><Text v-if="visible" :text="title"/>'
            '<Part v-else=""/></Column></template>')
        roots = json.loads(manifest.read_text())["roots"]
        self.assertIn("CodeEditor", roots)
        self.assertIn("Text", roots)
        self.assertNotIn("TimeSeriesChart", roots)

    def test_implicit_composite_dependencies(self):
        manifest, _ = self.compile('<template view-model="Root"><SettingsPage><Text text="x"/></SettingsPage></template>')
        components = json.loads(manifest.read_text())["components"]
        self.assertTrue({"Column", "Row", "Text", "Scroll", "Content"}.issubset(components))

    def test_multiple_views_and_missing_component_guard(self):
        a, _ = self.compile('<template view-model="Root"><Button text="x"/></template>')
        b, _ = self.compile('<template view-model="Root"><Progress value="0.5"/></template>', "Other")
        header, selected = merger.merge([a, b])
        self.assertIn("Button", selected)
        self.assertIn("Progress", selected)
        self.assertIn("#define ONEUI_COMPONENT_ENABLED_CodeEditor 0", header)
        self.assertIn("#define ONEUI_COMPONENT_ENABLED_Button 1", header)
        self.assertEqual(header, merger.merge([b, a])[0])

    def test_dynamic_and_external_components_keep_full_runtime(self):
        manifest, _ = self.compile('<template view-model="Root"><Text text="x"/></template>')
        header, _ = merger.merge([manifest], extra=["CodeEditor"])
        self.assertIn("#define ONEUI_COMPONENT_ENABLED_TimeSeriesChart 1", header)
        data = json.loads(manifest.read_text())
        data["full_runtime"] = True
        manifest.write_text(json.dumps(data))
        self.assertEqual(header, merger.merge([manifest])[0])
        with self.assertRaisesRegex(ValueError, "Unknown dynamic components"):
            merger.merge([manifest], extra=["Typo"])

    def test_sidecar_does_not_change_ir_or_language_adapters(self):
        manifest, output = self.compile('<template view-model="Root"><Text :text="title"/></template>')
        before = output.read_bytes()
        subprocess.run([str(COMPILER), "--input", str(self.root / "Page.one"), "--schema",
                        str(self.root / "Root.oneschema"), "--output", str(output), "--emit", "ir"], check=True)
        self.assertEqual(before, output.read_bytes())
        expected = manifest.read_bytes()
        self.compile('<template view-model="Root"><Text :text="title"/></template>', emit="rust")
        self.assertEqual(expected, manifest.read_bytes())


if __name__ == "__main__":
    unittest.main()

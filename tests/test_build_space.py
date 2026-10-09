import contextlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import Mock, patch

spec = importlib.util.spec_from_file_location("build_space", Path(__file__).resolve().parents[1] / "scripts/check-build-space.py")
space = importlib.util.module_from_spec(spec)
spec.loader.exec_module(space)


class BuildSpaceTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name)
        redirects = contextlib.ExitStack()
        self.addCleanup(redirects.close)
        redirects.enter_context(contextlib.redirect_stdout(io.StringIO()))
        redirects.enter_context(contextlib.redirect_stderr(io.StringIO()))

    def check(self, free, system="Linux", release="native", host=None):
        with patch.object(space.shutil, "disk_usage", return_value=Mock(free=free)), \
             patch.object(space.platform, "system", return_value=system), \
             patch.object(space.platform, "release", return_value=release):
            return space.check_space([self.path / "future" / "output"], 4, host_query=host)

    def test_nonexistent_output_uses_parent_without_creating_it(self):
        self.assertEqual(space.existing_parent(self.path / "future" / "output"), self.path.resolve())
        self.assertFalse((self.path / "future").exists())

    def test_native_low_space_is_rejected(self):
        self.assertFalse(self.check(space.GIB))

    def test_macos_space_check_needs_no_windows_tools(self):
        self.assertTrue(self.check(8 * space.GIB, system="Darwin"))

    def test_large_wsl_virtual_capacity_does_not_hide_full_host(self):
        self.assertFalse(self.check(930 * space.GIB, release="6.6-microsoft-standard-WSL2",
                                    host=lambda: ("C:\\WSL", 0)))

    def test_wsl_backing_volume_can_be_on_another_drive(self):
        self.assertTrue(self.check(930 * space.GIB, release="6.6-microsoft-standard-WSL2",
                                   host=lambda: ("D:\\WSL", 8 * space.GIB)))

    def test_wsl_host_errors_fail_closed(self):
        with self.assertRaises(subprocess.TimeoutExpired):
            self.check(930 * space.GIB, release="microsoft", host=Mock(side_effect=subprocess.TimeoutExpired("powershell", 20)))

    def test_host_lookup_quotes_distro_and_reads_actual_free_space(self):
        response = Mock(stdout=json.dumps({"path": "D:\\WSL", "free": 6 * space.GIB}))
        with patch.object(space.shutil, "which", return_value="powershell.exe"), \
             patch.dict(space.os.environ, {"WSL_DISTRO_NAME": "Ubuntu'custom"}), \
             patch.object(space.subprocess, "run", return_value=response) as run:
            self.assertEqual(space.wsl_backing_volume(), ("D:\\WSL", 6 * space.GIB))
            self.assertIn("'Ubuntu''custom'", run.call_args.args[0][-1])
            self.assertFalse(run.call_args.kwargs.get("shell", False))

    def test_invalid_host_response_is_rejected(self):
        with patch.object(space.shutil, "which", return_value="powershell.exe"), \
             patch.dict(space.os.environ, {"WSL_DISTRO_NAME": "Ubuntu"}), \
             patch.object(space.subprocess, "run", return_value=Mock(stdout='{"path":"C:","free":-1}')):
            with self.assertRaises(RuntimeError):
                space.wsl_backing_volume()


if __name__ == "__main__":
    unittest.main()

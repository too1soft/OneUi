import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from windows_pe import PE, PEError

spec = importlib.util.spec_from_file_location("audit", Path(__file__).resolve().parents[1] / "scripts/audit-windows-runtime.py")
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


def fixture(arch="x86", symbol="GetTickCount", delay=False, ordinal=False, module="KERNEL32.dll"):
    data = bytearray(0x1200)
    def put(offset, fmt, *values):
        struct.pack_into("<" + fmt, data, offset, *values)
    def rva(address):
        return address - 0x1000 + 0x200
    def string(address, value):
        value = value.encode("ascii") + b"\0"
        data[rva(address):rva(address) + len(value)] = value
    data[:2] = b"MZ"
    put(0x3C, "I", 0x80)
    data[0x80:0x84] = b"PE\0\0"
    wide = arch == "x64"
    size = 240 if wide else 224
    put(0x84, "HH", 0x8664 if wide else 0x14C, 1)
    put(0x94, "H", size)
    opt = 0x98
    put(opt, "H", 0x20B if wide else 0x10B)
    put(opt + (24 if wide else 28), "Q" if wide else "I", 0x400000)
    put(opt + 48, "HH", 6, 1)
    put(opt + 60, "I", 0x200)
    directory = opt + (112 if wide else 96)
    put(directory - 4, "I", 16)
    put(opt + size + 8, "IIII", 0x1000, 0x1000, 0x1000, 0x200)
    if delay:
        put(directory + 13 * 8, "II", 0x1000, 64)
        put(rva(0x1000), "IIIIIIII", 1, 0x1200, 0, 0x1100, 0x1100, 0, 0, 0)
    else:
        put(directory + 8, "II", 0x1000, 40)
        put(rva(0x1000), "IIIII", 0x1100, 0, 0, 0x1200, 0x1100)
    value = ((1 << (63 if wide else 31)) | 12) if ordinal else 0x1300
    put(rva(0x1100), "Q" if wide else "I", value)
    string(0x1200, module)
    string(0x1302, symbol)
    return data


class WindowsAuditTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / "candidate.exe"

    def read(self, **kwargs):
        self.path.write_bytes(fixture(**kwargs))
        return PE(self.path)

    def reference(self, arch="x86"):
        return {"label": "test only", "architecture": arch, "maximumSubsystemVersion": [6, 1],
                "modules": {"kernel32.dll": {"exports": {"GetTickCount": None, "#12": None}}}}

    def test_both_architectures_and_import_tables(self):
        for arch in ("x86", "x64"):
            for delay in (False, True):
                pe = self.read(arch=arch, delay=delay)
                self.assertEqual(pe.arch, arch)
                self.assertEqual(list(pe.imports()), [{"module": "kernel32.dll", "symbol": "GetTickCount", "kind": "delay" if delay else "normal"}])
                self.assertTrue(audit.audit([self.path], self.reference(arch))["passed"])

    def test_old_failure_rejected_for_normal_and_delay_imports(self):
        for delay in (False, True):
            self.read(symbol="GetSystemTimePreciseAsFileTime", delay=delay)
            report = audit.audit([self.path], self.reference())
            self.assertFalse(report["passed"])
            self.assertIn("GetSystemTimePreciseAsFileTime", report["errors"][0])

    def test_ordinal_imports_are_checked(self):
        for arch in ("x86", "x64"):
            pe = self.read(arch=arch, ordinal=True)
            self.assertEqual(list(pe.imports())[0]["symbol"], "#12")
            ref = self.reference(arch)
            self.assertTrue(audit.audit([self.path], ref)["passed"])
            del ref["modules"]["kernel32.dll"]["exports"]["#12"]
            self.assertFalse(audit.audit([self.path], ref)["passed"])

    def test_architecture_mismatch_is_rejected(self):
        self.read(arch="x64")
        self.assertFalse(audit.audit([self.path], self.reference())["passed"])

    def test_new_subsystem_is_rejected(self):
        data = fixture()
        struct.pack_into("<HH", data, 0x98 + 48, 10, 0)
        self.path.write_bytes(data)
        self.assertFalse(audit.audit([self.path], self.reference())["passed"])

    def test_unknown_api_is_not_silently_approved(self):
        self.read(symbol="UnreviewedNewWindowsAPI")
        self.assertFalse(audit.audit([self.path], self.reference())["passed"])

    def test_runtime_and_api_set_are_not_broadly_allowlisted(self):
        for module in ("api-ms-win-core-timezone-l1-1-0.dll", "vcruntime140.dll", "ucrtbase.dll", "msvcp140.dll"):
            self.read(module=module)
            # An app-local copy must not evade the prerequisite check.
            (self.path.parent / module).write_bytes(fixture())
            report = audit.audit([self.path], self.reference())
            self.assertFalse(report["passed"])
            self.assertIn("unsupported runtime/API-set", report["errors"][0])

    def test_missing_bundled_library_fails(self):
        self.read(module="oneui.dll")
        self.assertIn("missing non-system dependency", audit.audit([self.path], self.reference())["errors"][0])

    def test_localized_compiler_include_lines(self):
        wrapper_spec = importlib.util.spec_from_file_location("wrapper", Path(__file__).resolve().parents[1] / "scripts/msvc-includes-wrapper.py")
        wrapper = importlib.util.module_from_spec(wrapper_spec)
        wrapper_spec.loader.exec_module(wrapper)
        prefix = "注意: 包含文件:".encode("gbk")
        self.assertEqual(wrapper.normalized(prefix + b"   D:\\include\\header.h\r\n", prefix),
                         b"Note: including file:   D:\\include\\header.h\r\n")
        diagnostic = b"file.cpp(4): error C0000: example\r\n"
        self.assertEqual(wrapper.normalized(diagnostic, prefix), diagnostic)

    def test_forwarders_resolve_and_cycles_fail_closed(self):
        modules = {"kernel32.dll": {"exports": {"A": "KERNELBASE.B"}},
                   "kernelbase.dll": {"exports": {"B": "NTDLL.#12"}},
                   "ntdll.dll": {"exports": {"#12": None}}}
        self.assertTrue(audit.resolve_export(modules, "kernel32.dll", "A"))
        modules["ntdll.dll"]["exports"]["#12"] = "KERNEL32.A"
        self.assertFalse(audit.resolve_export(modules, "kernel32.dll", "A"))
        del modules["ntdll.dll"]
        self.assertFalse(audit.resolve_export(modules, "kernel32.dll", "A"))

    def test_malformed_inputs_fail_closed(self):
        for data in (b"", b"MZ", fixture()[:0x300]):
            self.path.write_bytes(data)
            self.assertFalse(audit.audit([self.path], self.reference())["passed"])
        data = fixture()
        struct.pack_into("<I", data, 0x98 + 96 + 8, 0x90000000)
        self.path.write_bytes(data)
        self.assertFalse(audit.audit([self.path], self.reference())["passed"])

    def test_legacy_delay_va_table(self):
        data = fixture(delay=True)
        struct.pack_into("<IIIIIIII", data, 0x200, 0, 0x401200, 0, 0x401100, 0x401100, 0, 0, 0)
        struct.pack_into("<I", data, 0x300, 0x401300)
        self.path.write_bytes(data)
        self.assertEqual(list(PE(self.path).imports())[0]["symbol"], "GetTickCount")


if __name__ == "__main__":
    unittest.main()

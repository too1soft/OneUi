#!/usr/bin/env python3
"""Check the real C/Rust ABI; optionally prove rejection using isolated fixtures.

Reuses the existing Rust target directory and never modifies product sources.
Only compact results are retained; TemporaryDirectory removes fixture sources.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import time


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "bindings/rust/oneui-sys/src"
MUTATIONS = (
    (
        "function_return_type",
        "pub fn oneui_utf8_abi_version() -> c_uint;",
        "pub fn oneui_utf8_abi_version() -> f64;",
        "compiler",
    ),
    (
        "function_parameter_type",
        "pub fn oneui_label_create_utf8(text: OneUiUtf8String)",
        "pub fn oneui_label_create_utf8(text: *const OneUiUtf8String)",
        "compiler",
    ),
    (
        "named_callback_parameter",
        'pub type OneUiClientSizeChangedCallback =\n'
        '    Option<unsafe extern "C" fn(width: c_float, height: c_float, user_data: *mut c_void)>;',
        'pub type OneUiClientSizeChangedCallback =\n'
        '    Option<unsafe extern "C" fn(width: f64, height: c_float, user_data: *mut c_void)>;',
        "compiler",
    ),
    (
        "same_size_field_order",
        "pub struct OneUiRect {\n    pub x: f32,\n    pub y: f32,",
        "pub struct OneUiRect {\n    pub y: f32,\n    pub x: f32,",
        "layout",
    ),
)


def invoke(environment: dict[str, str]) -> tuple[int, str, float]:
    start = time.monotonic()
    result = subprocess.run(
        ["cargo", "build" if os.name == "nt" else "run", "--manifest-path", str(ROOT / "bindings/rust/Cargo.toml"),
         "-p", "oneui-abi-tests", "--locked"],
        cwd=ROOT, env=environment, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        encoding="utf-8", errors="replace", check=False,
    )
    if os.name == "nt" and result.returncode == 0:
        # Windows searches the executable's directory before PATH. A DLL left
        # by another Rust consumer can silently override the SDK being tested.
        # Run just this executable in a bounded, empty fixture; reuse the SDK
        # DLL through PATH and remove the executable immediately afterwards.
        target = Path(environment["CARGO_TARGET_DIR"])
        with tempfile.TemporaryDirectory(prefix="oneui-abi-run-", dir=target) as temporary:
            executable = Path(temporary) / "oneui-abi-tests.exe"
            shutil.copy2(target / "debug/oneui-abi-tests.exe", executable)
            checked = subprocess.run([str(executable)], cwd=ROOT, env=environment,
                                     stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                     encoding="utf-8", errors="replace", check=False)
            result = subprocess.CompletedProcess(result.args, checked.returncode,
                                                 result.stdout + checked.stdout)
    return result.returncode, result.stdout, round(time.monotonic() - start, 3)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--oneui-lib-dir", type=Path, default=os.environ.get("ONEUI_LIB_DIR"))
    parser.add_argument("--negative", action="store_true", help="also inject four incompatible declarations")
    parser.add_argument("--report", type=Path, help="overwrite a compact JSON result at this path")
    args = parser.parse_args()
    environment = os.environ.copy()
    # Normal checks always use the checked-out source, not an inherited fixture.
    environment.pop("ONEUI_ABI_BINDINGS", None)
    target = Path(environment.get("CARGO_TARGET_DIR", ROOT / "bindings/rust/target")).resolve()
    target.mkdir(parents=True, exist_ok=True)
    environment["CARGO_TARGET_DIR"] = str(target)
    environment["CARGO_INCREMENTAL"] = "0"
    library = args.oneui_lib_dir
    if library is None and os.name == "nt":
        library = ROOT / "build/msvc-bundled-static-release"
    if library is not None:
        library = library.resolve(strict=True)
        environment["ONEUI_LIB_DIR"] = str(library)
        loader_path = "PATH" if os.name == "nt" else ("DYLD_LIBRARY_PATH" if os.sys.platform == "darwin" else "LD_LIBRARY_PATH")
        environment[loader_path] = str(library) + os.pathsep + environment.get(loader_path, "")

    results: list[dict] = []

    def baseline(name: str) -> bool:
        code, output, seconds = invoke(environment)
        layout = re.search(r"PASSED (\d+) tests", output)
        signatures = re.search(r"PASSED (\d+) compiler-checked function signatures", output)
        passed = code == 0 and layout is not None and signatures is not None
        results.append({"name": name, "passed": passed, "exit_code": code, "seconds": seconds,
                        "layout_checks": int(layout[1]) if layout else None,
                        "function_signatures": int(signatures[1]) if signatures else None,
                        "diagnostic": output[-6000:]})
        print(f"{name}: {'PASS' if passed else 'FAIL'}", flush=True)
        return passed

    passed = baseline("baseline")
    if passed and args.negative:
        try:
            for name, before, after, phase in MUTATIONS:
                with tempfile.TemporaryDirectory(prefix="oneui-abi-fixture-", dir=target) as temporary:
                    fixture = Path(temporary) / "src"
                    shutil.copytree(SOURCE, fixture)
                    source = fixture / "lib.rs"
                    original = source.read_text(encoding="utf-8")
                    if original.count(before) != 1:
                        raise RuntimeError(f"{name}: expected exactly one mutation site")
                    source.write_text(original.replace(before, after), encoding="utf-8")
                    fixture_environment = environment | {"ONEUI_ABI_BINDINGS": str(source)}
                    code, output, seconds = invoke(fixture_environment)
                    # A missing compiler/DLL is not a successful negative test.
                    expected_failure = (
                        "signatures.c" in output and bool(re.search(r"error(?: C\d+|:)", output))
                        if phase == "compiler" else "OneUiRect" in output and "offset" in output.lower()
                    )
                    rejected = code != 0 and expected_failure
                    passed = rejected and passed
                    results.append({"name": name, "passed": rejected, "exit_code": code,
                                    "seconds": seconds, "expected_phase": phase,
                                    "diagnostic": output[-6000:]})
                    print(f"{name}: {'REJECTED as expected' if rejected else 'FAIL'}", flush=True)
        finally:
            # Restore generated code and the executable even if a fixture failed.
            passed = baseline("restored_baseline") and passed
    report = {"passed": passed, "target": str(target), "library": str(library),
              "negative_tests_requested": args.negative, "results": results}
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    if not passed:
        for result in results:
            if not result["passed"]:
                print(result["diagnostic"])
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())

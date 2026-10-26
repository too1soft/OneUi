"""Compile and run the safe Rust bindings from each shipped SDK, without SDK sources."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import zipfile

p = argparse.ArgumentParser()
for field in ("packages", "work", "cargo-target", "report"):
    p.add_argument("--" + field, type=Path, required=True)
p.add_argument("--target", required=True)
p.add_argument("--version", help="Select an immutable version when historical packages coexist")
p.add_argument("--cargo", default="cargo")
p.add_argument("--rust-target", default="")
args = p.parse_args()
root = args.work.resolve()
if root.name != "rust-sdk-consumer-current" or root.is_symlink():
    raise ValueError("Use the owned rust-sdk-consumer-current fixture")
if root.exists() and not (root / ".oneui-owned-fixture").is_file():
    raise ValueError("Refusing an unknown fixture")
results = []
args.report.parent.mkdir(parents=True, exist_ok=True)
with args.report.with_suffix(".log").open("w", encoding="utf-8") as log:
    for linkage in ("shared", "static"):
        if root.exists():
            shutil.rmtree(root)
        root.mkdir(parents=True)
        (root / ".oneui-owned-fixture").write_text("OneUI packaged Rust consumer")
        prefix = "oneui-" + args.version if args.version else "*"
        packages = list(args.packages.glob(prefix + "-" + args.target + "-" + linkage + ".zip"))
        if len(packages) != 1:
            raise ValueError("Select exactly one SDK release")
        sdk, project = root / "sdk", root / "consumer"
        with zipfile.ZipFile(packages[0]) as archive:
            for member in archive.namelist():
                if Path(member).is_absolute() or ".." in Path(member).parts or "\\" in member:
                    raise ValueError("Unsafe zip member")
            archive.extractall(sdk)
        (project / "src").mkdir(parents=True)
        manifest = json.loads((sdk / "sdk-manifest.json").read_text())
        dependency = json.dumps((sdk / "bindings/rust/oneui").as_posix())
        (project / "Cargo.toml").write_text(
            '[package]\nname="oneui-package-consumer"\nversion="0.0.0"\nedition="2021"\n'
            '[dependencies]\noneui={path=' + dependency + ', features=' +
            ('["static-link"]' if linkage == "static" else '[]') + '}\n'
            '[profile.dev]\ndebug=0\nincremental=false\n')
        (project / "src/main.rs").write_text(
            'fn main() {\n'
            ' assert_eq!(unsafe { oneui::sys::oneui_utf8_abi_version() }, oneui::sys::UTF8_ABI_VERSION);\n'
            ' let window = oneui::Window::new(&oneui::WindowOptions { title: "SDK Rust consumer".into(),'
            ' visible: false, ..Default::default() }).expect("create native window");\n'
            ' window.set_title("Packaged safe Rust API");\n'
            ' drop(window);\n'
            ' println!("packaged safe Rust consumer PASS");\n}\n')
        env = dict(os.environ)
        env.update(ONEUI_SDK_DIR=str(sdk),
                   ONEUI_SDK_MANIFEST_SHA256=hashlib.sha256((sdk/"sdk-manifest.json").read_bytes()).hexdigest(),
                   CARGO_TARGET_DIR=str(args.cargo_target.resolve()), CARGO_INCREMENTAL="0")
        env["PATH"] = str(sdk/"bin") + os.pathsep + str(Path(args.cargo).parent) + os.pathsep + env["PATH"]
        if os.name == "nt":
            env["RUSTFLAGS"] = "-C target-feature=+crt-static" if linkage == "static" else ""
        elif platform.system() == "Darwin":
            env["DYLD_LIBRARY_PATH"] = str(sdk/"lib")
        else:
            env.update(LD_LIBRARY_PATH=str(sdk/"lib"), ONEUI_LINUX_BACKEND="x11")
        command = [args.cargo, "build", "--offline", "--manifest-path", str(project/"Cargo.toml")]
        if args.rust_target:
            command += ["--target", args.rust_target]
        run = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, env=env, timeout=240)
        item = dict(target=args.target, rustTarget=args.rust_target, linkage=linkage, compileExit=run.returncode)
        results.append(item)
        if run.returncode:
            args.report.write_text(json.dumps(results, indent=2))
            raise SystemExit(run.returncode)
        binary = args.cargo_target / args.rust_target / "debug" / ("oneui-package-consumer" + (".exe" if os.name=="nt" else ""))
        command = [str(binary.resolve())]
        if platform.system() == "Linux":
            command = ["xvfb-run", "-a"] + command
        run = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, env=env, timeout=30)
        item["runExit"] = run.returncode
        args.report.write_text(json.dumps(results, indent=2) + "\n")
        if run.returncode:
            raise SystemExit(run.returncode)
        shutil.rmtree(root)
print(json.dumps(results))

"""Verify the packaged CMake consumers using a single bounded extraction directory."""
import argparse
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import zipfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--packages", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--target", required=True)
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    root = args.work.resolve()
    if root == Path(root.anchor) or root.name != "sdk-consumer-current":
        raise ValueError("Work directory must be the dedicated sdk-consumer-current")
    if root.exists() and not (root / ".oneui-owned-fixture").is_file():
        raise ValueError("Refusing to reuse a non-owned directory")
    result = []
    log_path = args.report.with_suffix(".log")
    args.report.parent.mkdir(parents=True, exist_ok=True)
    with log_path.open("w", encoding="utf-8") as log:
        for linkage in ("shared", "static"):
            package = list(args.packages.glob("*-" + args.target + "-" + linkage + ".zip"))
            if len(package) != 1:
                raise ValueError("Expected exactly one versioned package per target")
            if root.exists():
                shutil.rmtree(root)
            root.mkdir(parents=True)
            (root / ".oneui-owned-fixture").write_text("Generated SDK consumer fixture")
            sdk, build = root / "sdk", root / "build"
            with zipfile.ZipFile(package[0]) as archive:
                for entry in archive.infolist():
                    rel = Path(entry.filename)
                    if rel.is_absolute() or ".." in rel.parts or "\\" in entry.filename:
                        raise ValueError("Unsafe archive member")
                archive.extractall(sdk)
            env = dict(os.environ)
            if platform.system() == "Darwin":
                env["PATH"] = str(Path(args.cmake).parent) + os.pathsep + env["PATH"]
            commands = [
                [args.cmake, "-S", str(sdk / "examples"), "-B", str(build), "-G", "Ninja",
                 "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_PREFIX_PATH=" + str(sdk)],
                [args.cmake, "--build", str(build), "--config", "Release", "-j", "2"]
            ]
            for command in commands:
                log.write(json.dumps(command) + "\n"); log.flush()
                run = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, env=env, timeout=180)
                if run.returncode:
                    result.append(dict(target=args.target, linkage=linkage, stage="compile", exit=run.returncode))
                    args.report.write_text(json.dumps(result, indent=2))
                    raise SystemExit(run.returncode)
            for name in ("oneui_c", "oneui_cpp"):
                binary = build / (name + (".exe" if os.name == "nt" else ""))
                command = [str(binary)]
                if platform.system() == "Linux":
                    command = ["xvfb-run", "-a", str(binary)]
                    env.update(ONEUI_LINUX_BACKEND="x11")
                log.write(json.dumps(command) + "\n"); log.flush()
                run = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, env=env, timeout=30)
                result.append(dict(target=args.target, linkage=linkage, consumer=name, exit=run.returncode))
                if run.returncode:
                    args.report.write_text(json.dumps(result, indent=2))
                    raise SystemExit(run.returncode)
            shutil.rmtree(root)
    args.report.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result))


if __name__ == "__main__":
    main()

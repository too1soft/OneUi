"""Create immutable, relocatable SDK zip files without staging full library copies."""
import argparse
import json
import platform
import re
import shlex
import zipfile
from pathlib import Path
from sdk_distribution import (bytes_hash, cmake_files, collect_bindings, digest,
                              examples_cmake, inventory)


def posix_closure(build, target_platform):
    runtime = "liboneui.dylib" if target_platform == "macos" else "liboneui.so"
    ninja = (build / "build.ninja").read_text()
    block = ninja.split("build " + runtime + ":", 1)[1]
    line = re.search(r"  LINK_LIBRARIES = (.*)", block)[1]
    tokens = shlex.split(line)
    archives = [build / "liboneui_static.a"]
    system, frameworks = [], []
    for i, token in enumerate(tokens):
        if token.endswith(".a"):
            candidate = Path(token)
            archives.append(candidate if candidate.is_absolute() else build / candidate)
        elif token == "-framework":
            frameworks.append(tokens[i + 1])
        elif token.startswith("-framework "):
            frameworks.append(token.split(" ", 1)[1])
        elif token.endswith(".framework"):
            frameworks.append(Path(token).stem)
        elif token.startswith("-l") and not token.startswith("-link"):
            system.append(token[2:])
        elif ".so" in token and token.startswith("/usr/"):
            system.append(Path(token).name.removeprefix("lib").split(".so")[0])
    system += ["c++", "objc"] if target_platform == "macos" else ["stdc++", "m", "dl", "pthread"]
    archives = list(dict.fromkeys(archives))
    if len({p.name for p in archives}) != len(archives):
        raise ValueError("Archive filename collision")
    for item in [build / runtime] + archives:
        if not item.is_file():
            raise ValueError("Missing binary " + str(item))
    cache = (build / "CMakeCache.txt").read_text()
    for flag in ["ONEUI_ENABLE_DEVELOPER_TOOLS", "ONEUI_ENABLE_TEST_FRAME_CAPTURE"]:
        if flag + ":BOOL=OFF" not in cache:
            raise ValueError("Production package requires " + flag + "=OFF")
    if not any(p.name == "libyogacore.a" for p in archives):
        raise ValueError("Expected current Yoga closure")
    return runtime, archives, list(dict.fromkeys(system)), list(dict.fromkeys(frameworks))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for arg in ("source", "output", "licenses"):
        parser.add_argument("--" + arg, type=Path, required=True)
    parser.add_argument("--input", type=Path)
    parser.add_argument("--build", type=Path)
    for arg in ("platform", "architecture", "profile", "toolchain", "minimum-os", "version"):
        parser.add_argument("--" + arg, required=True)
    parser.add_argument("--source-commit", default="")
    args = parser.parse_args()
    source, output = args.source.resolve(), args.output.resolve()
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+(?:-[a-zA-Z0-9.-]+)?", args.version):
        raise ValueError("Use a semantic release version")
    abi = int(re.search(r"#define ONEUI_UTF8_ABI_VERSION (\d+)u?",
                        (source / "include/oneui/oneui_c_api.h").read_text())[1])
    output.mkdir(parents=True, exist_ok=True)
    frozen = None
    if args.platform == "windows":
        if not args.input:
            raise ValueError("Windows packages require a frozen SDK input")
        frozen = json.loads((args.input / "sdk-manifest.json").read_text())
        if any(frozen[key] != expected for key, expected in
               (("abiVersion", abi), ("architecture", args.architecture), ("profile", args.profile))):
            raise ValueError("Frozen SDK does not match the requested ABI/target")
        for entry in frozen["files"]:
            p = args.input / entry["path"]
            if p.stat().st_size != entry["size"] or digest(p) != entry["sha256"]:
                raise ValueError("Frozen input mismatch: " + entry["path"])
        archives = [args.input / "lib" / (name + ".lib") for name in frozen["staticLibraries"]]
        runtime = args.input / "bin/oneui.dll"
        system, frameworks = frozen["systemLibraries"], []
        headers = args.input / "include"
        commit, dirty = frozen["sourceCommit"], frozen["sourceDirty"]
    else:
        if args.platform == "macos" and platform.system() != "Darwin":
            raise ValueError("Package Mac binaries on their native build host")
        if args.platform == "linux" and platform.system() != "Linux":
            raise ValueError("Package Linux binaries on their build host")
        runtime_name, archives, system, frameworks = posix_closure(args.build, args.platform)
        runtime, headers = args.build / runtime_name, source / "include"
        commit, dirty = args.source_commit, True
        if not re.fullmatch("[0-9a-f]{40}", commit):
            raise ValueError("Provide the actual source synchronization commit; file hashes also recorded")
    for p in headers.rglob("*"):
        if p.is_file() and digest(p) != digest(source / "include" / p.relative_to(headers)):
            raise ValueError("Public header differs from the selected binary input")
    common = {"include/" + p.relative_to(headers).as_posix(): p
              for p in headers.rglob("*") if p.is_file()}
    common.update(collect_bindings(source))
    common.update({"licenses/" + p.relative_to(args.licenses).as_posix(): p
                   for p in args.licenses.rglob("*") if p.is_file()})
    if not any(name.startswith("licenses/") for name in common):
        raise ValueError("License closure is empty")
    common["examples/main.c"] = source / "tests/sdk_consumer/main.c"
    common["examples/main.cpp"] = source / "tests/sdk_consumer/main.cpp"
    common["examples/CMakeLists.txt"] = examples_cmake()
    for doc in ("sdk-quick-start.md", "sdk-platforms.md", "sdk-rust.md", "sdk-release-guide.md"):
        common["docs/" + doc] = source / "docs" / doc
    common["README.md"] = ("# OneUI " + args.version +
        "\n\nUse cmake -S examples -B build -DCMAKE_PREFIX_PATH=\"<this SDK directory>\".\n"
        "Build your application only; OneUI and bundled dependencies are already compiled.\n"
        "See docs/sdk-quick-start.md and sdk-manifest.json. This release is a preview.\n").encode()
    artifacts = []
    for linkage in ("shared", "static"):
        entries = dict(common)
        if linkage == "shared":
            entries[("bin/" if args.platform == "windows" else "lib/") + runtime.name] = runtime
            if args.platform == "windows":
                entries["lib/oneui.lib"] = args.input / "lib/oneui.lib"
        else:
            entries.update({"lib/" + p.name: p for p in archives})
        entries["cmake/OneUIConfig.cmake"] = cmake_files(args.platform, args.architecture,
            linkage, [p.name for p in archives], system, frameworks, args.version)
        entries["cmake/OneUIConfigVersion.cmake"] = ('set(PACKAGE_VERSION "' + args.version.split("-", 1)[0] + '")\nif(PACKAGE_FIND_VERSION VERSION_GREATER PACKAGE_VERSION)\nset(PACKAGE_VERSION_COMPATIBLE FALSE)\nelse()\nset(PACKAGE_VERSION_COMPATIBLE TRUE)\nendif()\nif(PACKAGE_FIND_VERSION VERSION_EQUAL PACKAGE_VERSION)\nset(PACKAGE_VERSION_EXACT TRUE)\nendif()\n').encode()
        names = [p.stem.removeprefix("lib") if args.platform != "windows" else p.stem for p in archives]
        manifest = dict(schemaVersion=1, distributionVersion=1, version=args.version,
            platform=args.platform, architecture=args.architecture, profile=args.profile,
            configuration="Release", linkage=linkage, abiVersion=abi, sourceCommit=commit,
            sourceDirty=dirty, crt="static" if args.platform == "windows" else "system",
            staticLibraries=names if linkage == "static" else [], systemLibraries=system,
            frameworks=frameworks, minimumOS=args.minimum_os, toolchain=args.toolchain,
            files=inventory(entries))
        if frozen:
            manifest["upstreamManifestSha256"] = digest(args.input / "sdk-manifest.json")
            manifest["sourceTreeSha256"] = frozen["sourceTreeSha256"]
        else:
            manifest["publicHeadersSha256"] = bytes_hash(json.dumps(inventory(
                {k: v for k, v in entries.items() if k.startswith("include/")}), sort_keys=True).encode())
        encoded = (json.dumps(manifest, indent=2, ensure_ascii=False) + "\n").encode()
        entries["sdk-manifest.json"] = encoded
        target = "-".join([args.platform, args.profile, args.architecture, linkage])
        name = "oneui-" + args.version + "-" + target + ".zip"
        destination = output / name
        if destination.exists():
            raise ValueError("Refusing to overwrite versioned package " + str(destination))
        try:
            with zipfile.ZipFile(destination, "x", compression=zipfile.ZIP_DEFLATED,
                                 compresslevel=6, strict_timestamps=False) as archive:
                for relative, content in sorted(entries.items()):
                    if isinstance(content, bytes):
                        archive.writestr(relative, content)
                    else:
                        archive.write(content, relative)
        except BaseException:
            destination.unlink(missing_ok=True)
            raise
        artifacts.append(dict(id=target, platform=args.platform, architecture=args.architecture,
            profile=args.profile, linkage=linkage, file=name, size=destination.stat().st_size,
            sha256=digest(destination), manifestSha256=bytes_hash(encoded),
            minimumOS=args.minimum_os, toolchain=args.toolchain,
            signature="未进行正式发布签名", validation="预览包；消费者验证结果见交付报告",
            limitations=["完整原生发布验收尚未完成"], systemDependencies=system + frameworks))
    record = output / ("artifacts-" + "-".join([args.platform, args.profile, args.architecture]) + ".json")
    record.write_text(json.dumps(artifacts, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"artifacts": artifacts}, ensure_ascii=False))


if __name__ == "__main__":
    main()

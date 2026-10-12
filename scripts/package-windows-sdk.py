"""Package a complete C/C++ Windows SDK from CMake's actual link closure."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess


def digest(path):
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()


def inventory(root):
    result = []
    for path in sorted(root.rglob("*")):
        if path.is_symlink():
            raise ValueError(f"Linked input: {path}")
        if path.is_file():
            result.append(dict(path=path.relative_to(root).as_posix(),
                               size=path.stat().st_size, sha256=digest(path)))
    return result


def source_identity(source):
    entries = []
    for folder in ("include", "src", "cmake"):
        for item in inventory(source / folder):
            entries.append(dict(item, path=folder + "/" + item["path"]))
    path = source / "CMakeLists.txt"
    entries.append(dict(path=path.name, size=path.stat().st_size, sha256=digest(path)))
    entries.sort(key=lambda item: item["path"])
    encoded = json.dumps(entries, sort_keys=True, separators=(",", ":")).encode()
    return entries, hashlib.sha256(encoded).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("source", "build", "output", "licenses"):
        parser.add_argument("--" + option, required=True, type=Path)
    args = parser.parse_args()
    source, build, output, licenses = (getattr(args, key).resolve()
                                       for key in ("source", "build", "output", "licenses"))
    if output.exists():
        raise ValueError("Output already exists; reuse requires explicit candidate cleanup")
    recipe_path = build / "sdk-inputs-Release.json"
    recipe = json.loads(recipe_path.read_text(encoding="utf-8"))
    if recipe["schemaVersion"] != 1 or recipe["configuration"] != "Release":
        raise ValueError("Expected a Release SDK recipe")
    cache = (build / "CMakeCache.txt").read_text(encoding="utf-8")
    for flag in ("ONEUI_ENABLE_TEST_FRAME_CAPTURE", "ONEUI_ENABLE_DEVELOPER_TOOLS"):
        if f"{flag}:BOOL=OFF" not in cache:
            raise ValueError(f"Production SDK requires {flag}=OFF")
    if "CMAKE_MSVC_RUNTIME_LIBRARY:UNINITIALIZED=MultiThreaded" not in cache and \
            "CMAKE_MSVC_RUNTIME_LIBRARY:STRING=MultiThreaded" not in cache:
        raise ValueError("Expected static CRT")
    if recipe["profile"] == "legacy" and "14.44.35207" not in recipe["compiler"]:
        raise ValueError("Legacy SDK must use the pinned compiler")
    archives = list(dict.fromkeys(Path(name) for name in recipe["archives"]))
    if len({path.name.lower() for path in archives}) != len(archives):
        raise ValueError("Colliding archive filenames")
    yoga = "ONEUI_ENABLE_YOGA:BOOL=ON" in cache
    if yoga and "yogacore.lib" not in {path.name for path in archives}:
        raise ValueError("Yoga is enabled but missing from the static closure")
    if recipe["profile"] == "legacy" and "freetype2.lib" not in {p.name for p in archives}:
        raise ValueError("Legacy FreeType closure missing")
    notices = ["oneui-MIT.txt", "skia-BSD.txt", "icu.txt", "harfbuzz.txt",
               "freetype-FTL.txt", "freetype-license-selection.txt", "freetype-bdf.txt",
               "freetype-pcf.txt", "freetype-gzip-notice.txt", "harfbuzz-ms-use.txt",
               "libjpeg-turbo.md", "libjpeg-IJG.txt", "libpng.txt", "libwebp.txt",
               "libwebp-patents.txt", "zlib.txt", "expat.txt"]
    copies = [(path, "lib/" + path.name) for path in archives]
    copies += [(build / "oneui.dll", "bin/oneui.dll"),
               (build / "oneui-viewc.exe", "bin/oneui-viewc.exe"),
               (build / "oneui.lib", "lib/oneui.lib")]
    copies += [(licenses / name, "licenses/" + name) for name in notices]
    if yoga:
        copies.append((build / "licenses/Yoga-LICENSE.txt", "licenses/Yoga-LICENSE.txt"))
    copies += [(path, "include/" + path.relative_to(source / "include").as_posix())
               for path in sorted((source / "include").rglob("*")) if path.is_file()]
    for path, _ in copies:
        if path.is_symlink() or not path.is_file():
            raise ValueError(f"Missing or linked input: {path}")
    source_files, source_hash = source_identity(source)
    version = int(re.search(r"#define ONEUI_UTF8_ABI_VERSION (\d+)u",
                           (source / "include/oneui/oneui_c_api.h").read_text(encoding="utf-8"))[1])
    for path, name in copies:
        destination = output / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, destination)
    static_names = [path.stem for path in archives]
    (output / "static-libraries.txt").write_text("\n".join(static_names) + "\n", encoding="utf-8")
    (output / "system-libraries.txt").write_text("\n".join(recipe["systemLibraries"]) + "\n", encoding="utf-8")
    (output / "source-inputs.json").write_text(json.dumps(source_files, indent=2) + "\n", encoding="utf-8")
    manifest = {key: recipe[key] for key in ("schemaVersion", "profile", "architecture", "configuration")}
    manifest.update(kind="oneui-sdk", crt="static", abiVersion=version,
                    staticLibraries=static_names, systemLibraries=recipe["systemLibraries"],
                    features=dict(yoga=yoga, developerTools=False, testFrameCapture=False),
                    sourceCommit=subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip(),
                    sourceDirty=bool(subprocess.check_output(["git", "-C", str(source), "status", "--porcelain"], text=True)),
                    sourceInputs=inventory(source / "include"), sourceTreeSha256=source_hash,
                    compilerSha256=digest(Path(recipe["compiler"])), recipeSha256=digest(recipe_path),
                    packagerSha256=digest(Path(__file__)),
                    runtimeAcceptance="pending-product-integration", files=inventory(output))
    manifest_path = output / "sdk-manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(dict(path=str(output), abi=version, sourceTreeSha256=source_hash,
                          manifestSha256=digest(manifest_path))))


if __name__ == "__main__":
    main()

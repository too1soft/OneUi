"""Add an immutable SDK release while preserving all previous downloads."""
import argparse
from datetime import date
import json
import os
from pathlib import Path
import re
import tempfile
import zipfile
from sdk_distribution import bytes_hash, digest

TARGETS = ["windows-modern-x86", "windows-modern-x86_64", "windows-legacy-x86",
           "windows-legacy-x86_64", "macos-native-arm64", "linux-ubuntu24.04-x86_64"]


def build_release(directory, records, version, channel, published_at):
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+(?:-[a-zA-Z0-9.-]+)?", version):
        raise ValueError("Use a semantic release version")
    date.fromisoformat(published_at)
    artifacts, provenance = [], set()
    for sidecar in sorted(records.glob("artifacts-*.json")):
        for item in json.loads(sidecar.read_text(encoding="utf-8")):
            if not item["file"].startswith("oneui-" + version + "-"):
                continue
            if Path(item["file"]).name != item["file"] or "\\" in item["file"]:
                raise ValueError("Unsafe archive filename")
            file = directory / item["file"]
            if file.stat().st_size != item["size"] or digest(file) != item["sha256"]:
                raise ValueError("SDK archive differs from packaging record")
            with zipfile.ZipFile(file) as archive:
                raw = archive.read("sdk-manifest.json")
                manifest = json.loads(raw)
            if bytes_hash(raw) != item["manifestSha256"] or manifest["version"] != version:
                raise ValueError("SDK manifest differs from packaging record")
            for field in ("platform", "profile", "architecture", "linkage"):
                if manifest[field] != item[field]:
                    raise ValueError("SDK target differs from packaging record")
            provenance.add((manifest["abiVersion"], manifest["sourceCommit"], manifest["sourceDirty"]))
            artifacts.append(item)
    expected = {target + "-" + mode for target in TARGETS for mode in ("shared", "static")}
    if {a["id"] for a in artifacts} != expected or len(artifacts) != len(expected):
        raise ValueError("Incomplete or duplicate target set")
    if len(provenance) != 1:
        raise ValueError("SDKs must have the same ABI and source provenance")
    abi, commit, dirty = provenance.pop()
    return dict(version=version, channel=channel, abi=abi, publishedAt=published_at,
                docsVersion=version, sourceCommit=commit, sourceDirty=dirty,
                notes=["Windows 四种目标、macOS arm64、Linux x86_64，分别提供动态和静态包。",
                       "请按每个包的支持范围、验收结果和签名状态接入。",
                       "包含匹配头文件、CMake 入口、Rust 绑定、许可证及校验清单。业务项目无需编译 OneUI。"],
                artifacts=artifacts)


def merge_release(catalog, release, replace_unpublished=False):
    if catalog.get("schemaVersion") != 1:
        raise ValueError("Unsupported catalog schema")
    previous = next((r for r in catalog["releases"] if r["version"] == release["version"]), None)
    if previous == release:
        return catalog
    if previous and (not replace_unpublished or previous["channel"] == "stable"):
        raise ValueError("Version already exists; publish a new version. Only an unpublished preview may be replaced explicitly")
    kept = [r for r in catalog["releases"] if r["version"] != release["version"]]
    return dict(schemaVersion=1, releases=[release] + kept)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--directory", type=Path, required=True)
    p.add_argument("--records", type=Path, help="Packaging sidecars; defaults to directory")
    p.add_argument("--version", required=True)
    p.add_argument("--channel", choices=("stable", "preview", "experimental"), default="preview")
    p.add_argument("--published-at", default=date.today().isoformat())
    p.add_argument("--replace-unpublished", action="store_true",
                   help="Explicitly replace local candidate metadata before first distribution; never a published release")
    args = p.parse_args()
    path = args.directory / "releases.json"
    release = build_release(args.directory, args.records or args.directory,
                            args.version, args.channel, args.published_at)
    catalog = json.loads(path.read_text(encoding="utf-8")) if path.exists() else dict(schemaVersion=1, releases=[])
    merged = merge_release(catalog, release, args.replace_unpublished)
    # Keep an atomic catalog transaction; the server validates every historical download on restart.
    fd, temporary = tempfile.mkstemp(prefix=".releases-", suffix=".tmp", dir=args.directory)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as output:
            json.dump(merged, output, ensure_ascii=False, indent=2)
            output.write("\n")
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)
    print(json.dumps({"catalog": str(path), "releases": len(merged["releases"]),
                      "downloads": len(release["artifacts"])}))


if __name__ == "__main__":
    main()

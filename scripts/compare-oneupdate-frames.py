"""Compare real OneUpdate preview states using its native frame capture API.

Requires validation OneUI runtimes with frame capture enabled. This never
clicks Install or changes an installation. Production sizes are measured
separately with capture disabled.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess


def layout_digest(path):
    snapshot = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(snapshot.get("nodes"), list):
        raise ValueError("Unexpected OneUI layout snapshot")
    for node in snapshot["nodes"]:
        if node.get("effectiveVisible") is False:
            # Hidden controls still report in-flight color transitions. Their
            # target style and geometry remain compared; transient paint
            # colors cannot affect the captured frame.
            node.pop("paintForeground", None)
            node.pop("paintBackground", None)
    encoded = json.dumps(snapshot, sort_keys=True, separators=(",", ":")).encode()
    return hashlib.sha256(encoded).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--install-root", type=Path, required=True)
    parser.add_argument("--test-fixture-bundle", type=Path,
                        help="Use OneUpdate's cfg(test) mutation-draining capture branch")
    args = parser.parse_args()
    if args.install_root.exists():
        raise ValueError("Use an unused isolated installation path")
    if args.output.exists():
        raise ValueError("Refusing to overwrite prior comparison evidence")
    args.output.mkdir(parents=True)
    results = []
    for theme in ("minimal_light", "brand_focus", "midnight"):
        for state in ("preparing", "agreements", "installing", "failed", "completed", "launch_failed"):
            row = {"theme": theme, "state": state}
            for label, executable in (("baseline", args.baseline), ("candidate", args.candidate)):
                folder = args.output / label
                folder.mkdir(exist_ok=True)
                prefix = folder / f"{theme}-{state}"
                environment = dict(os.environ, ONEUPDATE_PREVIEW_THEME=theme, ONEUPDATE_PREVIEW_STATE=state,
                                   ONEUPDATE_CAPTURE_FRAME=str(prefix.with_suffix(".png").resolve()),
                                   ONEUPDATE_LAYOUT_SNAPSHOT=str(prefix.with_suffix(".json").resolve()))
                if args.test_fixture_bundle:
                    environment.update(ONEUI_ACCEPTANCE_FRAME_BUNDLE=str(args.test_fixture_bundle.resolve(strict=True)),
                                       ONEUI_ACCEPTANCE_FRAME_ROOT=str(args.install_root.resolve()))
                    command = [str(executable.resolve(strict=True)), "--ignored", "--exact",
                               "oneui_sdk_acceptance::actual_bundle_frame", "--test-threads=1"]
                else:
                    command = [str(executable.resolve(strict=True)), "--install-root", str(args.install_root.resolve())]
                subprocess.run(command, env=environment, check=True, timeout=30,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                row[label] = {suffix: hashlib.sha256(prefix.with_suffix(suffix).read_bytes()).hexdigest()
                              for suffix in (".png", ".json")}
                row[label]["layout"] = layout_digest(prefix.with_suffix(".json"))
            row["identical"] = all(row["baseline"][key] == row["candidate"][key] for key in (".png", "layout"))
            row["raw_layout_identical"] = row["baseline"][".json"] == row["candidate"][".json"]
            results.append(row)
    (args.output / "comparison.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
    mismatches = [row for row in results if not row["identical"]]
    print(json.dumps({"comparisons": len(results), "identical": len(results) - len(mismatches),
                      "mismatches": mismatches}))
    if mismatches:
        raise SystemExit(1)


if __name__ == "__main__":
    main()

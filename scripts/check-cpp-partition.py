#!/usr/bin/env python3
"""Verify the D12 source-preserving partition against its Git baseline.

This is a read-only migration proof, not a substitute for compilation, ABI or
behavior tests. It compares definitions verbatim and expands the Win32 includes
without running a C++ preprocessor. Only blank lines at include seams may differ.
"""

import argparse
from collections import defaultdict
import json
from pathlib import Path
import re
import subprocess


ROOT = Path(__file__).resolve().parents[1]
CAPI = "src/capi/oneui_c_api.cpp"
WIN32 = "src/platform/win32/window_win32.cpp"
FAMILIES = """window_properties window_shell layout snapshot services style_sheet
shell_layout containers terminal content button text_input charts table media
selection navigation overlays virtual_list tree status interactive style_apply
control_values""".split()
LITERALS = re.compile(
    r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\''
)


def git(*args):
    return subprocess.check_output(["git", "-C", str(ROOT), *args]).decode("utf-8")


def old(base, path):
    return git("show", f"{base}:{path}").replace("\r\n", "\n")


def current(path):
    return (ROOT / path).read_text(encoding="utf-8")


def definitions(text):
    masked = LITERALS.sub(
        lambda match: "".join("\n" if c == "\n" else " " for c in match[0]), text
    )
    result = defaultdict(list)
    for match in re.finditer(
        r"^([A-Za-z_][^\n{};]*?)\b([A-Za-z_]\w*)\s*\(", masked, re.M
    ):
        brace = masked.find("{", match.end())
        semi = masked.find(";", match.end())
        if brace < 0 or 0 <= semi < brace:
            continue
        depth, end = 1, brace + 1
        while depth:
            depth += (masked[end] == "{") - (masked[end] == "}")
            end += 1
        if masked[end:end + 1] != ";":
            start = match.start()
            previous = masked.rfind("\n", 0, max(0, start - 1)) + 1
            if masked[previous:start].strip().startswith("template "):
                start = previous
            result[match[2]].append(text[start:end])
    return result


def verify(base, public_base):
    before = definitions(old(base, CAPI))
    sources = [CAPI] + [f"src/capi/oneui_c_api_{name}.cpp" for name in FAMILIES]
    headers = [
        "src/capi/oneui_c_api_controls_internal.h",
        "src/capi/oneui_c_api_snapshot_internal.h",
    ]
    after = defaultdict(list)
    for path in sources + headers:
        for name, bodies in definitions(current(path)).items():
            after[name].extend(bodies)
    assert set(after) == set(before), f"Definition set changed: {set(after) ^ set(before)}"
    for name, bodies in before.items():
        assert sorted(after[name]) == sorted(bodies), f"Definition changed or duplicated: {name}"

    # No initializer, owner, lock or counter is duplicated across control TUs.
    registry = current(CAPI)
    all_sources = "\n".join(current(path) for path in sources)
    for declaration in (
        "std::weak_ptr<oneui::StyleSheet> gDefaultStyleSheet;",
        "std::mutex gWidgetRegistryMutex;",
        "std::vector<std::shared_ptr<OneUiWidgetStyleBinding>> gWidgetRegistry;",
    ):
        assert registry.count(declaration) == all_sources.count(declaration) == 1
    font_fragment = "src/capi/oneui_c_api_fonts.inc"
    assert current(font_fragment) == old(base, font_fragment), "Font definitions changed"

    includes = []
    def expand(match):
        path = "src/platform/win32/" + match[1]
        includes.append(path)
        return current(path)
    expanded = re.sub(
        r'^#include "(window_win32_[a-z_]+\.inc)"\n', expand, current(WIN32), flags=re.M
    )
    nonblank = lambda text: [line for line in text.splitlines() if line.strip()]
    assert nonblank(expanded) == nonblank(old(base, WIN32)), "Win32 expanded source changed"
    assert len(includes) == len(set(includes)) == 17, "Win32 include ownership changed"

    source_list = current("cmake/OneUISources.cmake")
    for path in sources:
        assert source_list.count(path + "\n") == 1, f"Source missing or duplicated: {path}"
    sizes = {}
    for path in sources + headers + includes + [WIN32]:
        data = (ROOT / path).read_bytes()
        text = data.decode("utf-8")
        assert not data.startswith(b"\xef\xbb\xbf") and "\ufffd" not in text, path
        sizes[path] = len(text.splitlines())
        assert sizes[path] <= 1000, f"File budget exceeded: {path} ({sizes[path]})"
    if public_base:
        assert not git("diff", "--name-only", public_base, "--", "include/"), "Public headers changed"
    return {
        "baseline": git("rev-parse", base).strip(),
        "win32_expanded_nonblank_text_identical": True,
        "c_export_definitions_identical": sum(len(v) for k, v in before.items() if k.startswith("oneui_")),
        "all_definition_bodies_identical": sum(map(len, before.values())),
        "public_headers_unchanged_from": public_base,
        "source_files": len(sizes),
        "max_source_lines": max(sizes.values()),
        "over_500": {path: count for path, count in sizes.items() if count > 500},
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", required=True, help="Git revision before the D12 partition")
    parser.add_argument("--public-base", help="Optional revision to compare all public headers against")
    args = parser.parse_args()
    print(json.dumps(verify(args.base, args.public_base), indent=2))

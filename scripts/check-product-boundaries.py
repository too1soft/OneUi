"""Check known product coupling in OneUI source/build files (not a full code audit).

By default scans tracked + untracked, non-ignored working files. --revision HEAD
checks the exact published candidate without hiding findings in a dirty checkout.
Historical documentation and attribution comments are intentionally not erased.
"""
import argparse
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
BRAND = re.compile(r"ishell\s*pro|wangyunchuan\.(?:cn|com)", re.I)
PRODUCT_DEP = re.compile(r"ishell[-_/](?:core|session|terminal|oneui)|(?:^|[/\\])ishellpro(?:[/\\]|$)", re.I)


def findings(path, text):
    if path.startswith(("docs/", "website/")):
        return []
    extension = pathlib.PurePosixPath(path).suffix
    if extension not in {".cpp", ".h", ".hpp", ".inc", ".rs", ".toml", ".cmake"} and path != "CMakeLists.txt":
        return []
    result = []
    block_comment = False
    for number, line in enumerate(text.splitlines(), 1):
        # Preserve line locations and attribution. This is a lexical guard, not
        # a language parser; it deliberately reports suspicious strings to review.
        if block_comment:
            if "*/" not in line:
                continue
            line = line.split("*/", 1)[1]
            block_comment = False
        while "/*" in line:
            before, rest = line.split("/*", 1)
            if "*/" in rest:
                line = before + rest.split("*/", 1)[1]
            else:
                line = before
                block_comment = True
                break
        if line.lstrip().startswith(("//", "# ")):
            continue
        if BRAND.search(line) or PRODUCT_DEP.search(line):
            result.append((number, "PRODUCT_REFERENCE", "Use neutral fixtures; keep product dependencies/brands in the downstream app."))
        if path.startswith("src/core/") and re.search(r'variant_\s*==\s*"terminal(?:-mac)?"', line):
            result.append((number, "PRODUCT_CHROME_BRANCH", "Product-selected fixed titlebar geometry belongs in the downstream app or a generic configurable API."))
    return result


def git(*args):
    return subprocess.check_output(["git", "-C", str(ROOT), *args])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--revision", help="Read files from a Git revision instead of the working tree")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        assert findings("src/core/a.cpp", 'const char* name="iShellPro";')
        assert findings("CMakeLists.txt", 'add_subdirectory("../ishellpro/native-shell")')
        assert findings("src/core/window_title_bar.cpp", 'if (variant_ == "terminal-mac") {}')
        assert not findings("src/platform/native_services.cpp", "IShellItem* item = nullptr;")
        assert not findings("src/core/outline_icons.inc", "// Generated from iShell approved icon paths.")
        assert not findings("docs/history.md", "iShellPro")
        print("Product boundary guard fixtures passed")
        return 0
    roots = ["src", "include", "bindings", "cmake", "tests", "CMakeLists.txt"]
    if args.revision:
        names = git("ls-tree", "-r", "--name-only", "-z", args.revision, "--", *roots).decode().split("\0")
    else:
        names = git("ls-files", "--cached", "--others", "--exclude-standard", "-z", "--", *roots).decode().split("\0")
    count = 0
    for name in sorted(set(names) - {""}):
        if pathlib.PurePosixPath(name).suffix not in {".cpp", ".h", ".hpp", ".inc", ".rs", ".toml", ".cmake"} and name != "CMakeLists.txt":
            continue
        if args.revision:
            text = git("show", args.revision + ":" + name).decode("utf-8", errors="replace")
        elif (ROOT / name).is_file():
            text = (ROOT / name).read_text(encoding="utf-8", errors="replace")
        else:
            continue
        for line, code, message in findings(name, text):
            print(f"{name}:{line}: [{code}] {message}")
            count += 1
    print(f"Product boundary scan: {count} findings ({args.revision or 'working tree'})")
    return 1 if count else 0


if __name__ == "__main__":
    sys.exit(main())

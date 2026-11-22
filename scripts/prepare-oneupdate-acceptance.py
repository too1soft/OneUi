"""Add a cfg(test)-only consumer fixture to a disposable OneUpdate source copy.

The caller supplies the SHA-256 of the unmodified main.rs from the pinned Git
commit (UTF-8, normalized LF, one final newline). No product behavior is changed.
"""
import argparse
import hashlib
from pathlib import Path
import shutil

HOOK = "\n#[cfg(all(test, windows))]\nmod oneui_sdk_acceptance;\n"
BEGIN = """            .dispatch_local(move || {
                if let Ok(snapshot_path) = std::env::var("ONEUPDATE_LAYOUT_SNAPSHOT") {"""
END = """                capture_window.close();
            })"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--expected-main-sha256", required=True)
    args = parser.parse_args()
    target = args.source.resolve() / "rust/crates/oneupdate-installer/src"
    path = target / "main.rs"
    source = path.read_text(encoding="utf-8-sig").replace(HOOK, "").rstrip() + "\n"
    if hashlib.sha256(source.encode()).hexdigest() != args.expected_main_sha256:
        raise ValueError("Consumer source differs from the explicitly pinned input")
    if source.count(BEGIN) != 1 or source.count(END) != 1:
        raise ValueError("Expected the pinned cfg(test) capture branch")
    # Keep the existing mutation-draining test branch; allow color transitions
    # to reach their target before recording a deterministic reference frame.
    source = source.replace(BEGIN, """            .dispatch_local(move || {
                let settle_window = Rc::clone(&capture_window);
                oneui_sdk_acceptance::capture_after_settling(settle_window, move || {
                if let Ok(snapshot_path) = std::env::var("ONEUPDATE_LAYOUT_SNAPSHOT") {""")
    source = source.replace(END, """                capture_window.close();
                });
            })""")
    path.write_text(source + HOOK, encoding="utf-8", newline="\n")
    fixture = Path(__file__).resolve().parents[1] / "tests/oneupdate_sdk_acceptance.rs"
    shutil.copy2(fixture, target / "oneui_sdk_acceptance.rs")
    print("Prepared cfg(test)-only fixture; product installer code path unchanged")


if __name__ == "__main__":
    main()

use std::{env, path::PathBuf, process::Command};
fn main() {
    let root = PathBuf::from(env::var_os("CARGO_MANIFEST_DIR").unwrap()).join("../../..");
    let viewc = env::var_os("ONEUI_VIEWC")
        .map(PathBuf::from)
        .unwrap_or_else(|| {
            root.join("build/capture").join(if cfg!(windows) {
                "oneui-viewc.exe"
            } else {
                "oneui-viewc"
            })
        });
    let out = PathBuf::from(env::var_os("OUT_DIR").unwrap());
    println!("cargo:rerun-if-env-changed=ONEUI_VIEWC");
    println!("cargo:rerun-if-changed={}", viewc.display());
    for extension in ["one", "oneschema"] {
        println!(
            "cargo:rerun-if-changed={}",
            root.join(format!("tests/viewc/Rust.{extension}")).display()
        );
    }
    let status = Command::new(&viewc)
        .arg("--emit")
        .arg("rust")
        .arg("--input")
        .arg(root.join("tests/viewc/Rust.one"))
        .arg("--schema")
        .arg(root.join("tests/viewc/Rust.oneschema"))
        .arg("--output")
        .arg(out.join("page.rs"))
        .arg("--ir-output")
        .arg(out.join("page.oneview"))
        .status()
        .expect("run host viewc");
    assert!(status.success(), "viewc failed");
    println!("cargo:rerun-if-changed={}", root.join("tests/viewc/RustWorkspace.one").display());
    let status = Command::new(&viewc)
        .args(["--emit", "ir", "--input"])
        .arg(root.join("tests/viewc/RustWorkspace.one"))
        .arg("--schema").arg(root.join("tests/viewc/Rust.oneschema"))
        .arg("--output").arg(out.join("workspace.oneview"))
        .status().expect("run workspace viewc");
    assert!(status.success(), "workspace viewc failed");
}

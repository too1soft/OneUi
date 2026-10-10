use std::{env, path::PathBuf, process::Command};
fn main() {
    let root = PathBuf::from(env::var_os("CARGO_MANIFEST_DIR").unwrap());
    let out = PathBuf::from(env::var_os("OUT_DIR").unwrap());
    let viewc = env::var_os("ONEUI_VIEWC")
        .map(PathBuf::from)
        .unwrap_or_else(|| root.join("../../build/capture/oneui-viewc.exe"));
    println!("cargo:rerun-if-env-changed=ONEUI_VIEWC");
    println!("cargo:rerun-if-changed={}", viewc.display());
    for ext in ["one", "oneschema"] {
        println!("cargo:rerun-if-changed=Workspace.{ext}");
    }
    assert!(Command::new(viewc)
        .args(["--emit", "rust", "--input"])
        .arg(root.join("Workspace.one"))
        .arg("--schema")
        .arg(root.join("Workspace.oneschema"))
        .arg("--output")
        .arg(out.join("workspace.rs"))
        .status()
        .expect("run OneUI view compiler")
        .success());
    if env::var_os("CARGO_FEATURE_ACCEPTANCE").is_some() {
        println!("cargo:rerun-if-env-changed=ONEUI_WORKSPACE_BRIDGE_DIR");
        let dir = env::var("ONEUI_WORKSPACE_BRIDGE_DIR")
            .expect("acceptance requires native test bridge; see README.md");
        println!("cargo:rustc-link-search=native={dir}");
        println!("cargo:rustc-link-lib=static=workspace_acceptance");
        println!("cargo:rerun-if-changed={dir}/workspace_acceptance.lib");
    }
}

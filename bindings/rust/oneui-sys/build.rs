use std::env;
use std::path::PathBuf;

fn main() {
    println!("cargo:rerun-if-env-changed=ONEUI_LIB_DIR");

    let lib_dir = env::var_os("ONEUI_LIB_DIR").map(PathBuf::from).unwrap_or_else(|| {
        panic!(
            "ONEUI_LIB_DIR is required and must point to the directory containing oneui.lib / liboneui.so"
        )
    });

    println!("cargo:rustc-link-search=native={}", lib_dir.display());
    println!("cargo:rustc-link-lib=dylib=oneui");
}

use std::env;
use std::fs;
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

    if env::var("CARGO_CFG_TARGET_OS").as_deref() == Ok("windows") {
        let runtime = lib_dir.join("oneui.dll");
        println!("cargo:rerun-if-changed={}", runtime.display());
        if runtime.is_file() {
            let out_dir = PathBuf::from(env::var_os("OUT_DIR").expect("OUT_DIR is required"));
            let profile_dir = out_dir
                .ancestors()
                .nth(3)
                .expect("OUT_DIR must be inside Cargo's profile directory");
            for destination in [profile_dir.to_path_buf(), profile_dir.join("deps")] {
                fs::create_dir_all(&destination).unwrap_or_else(|error| {
                    panic!(
                        "failed to create OneUI runtime directory {}: {error}",
                        destination.display()
                    )
                });
                fs::copy(&runtime, destination.join("oneui.dll")).unwrap_or_else(|error| {
                    panic!(
                        "failed to copy {} into {}: {error}",
                        runtime.display(),
                        destination.display()
                    )
                });
            }
        }
    }
}

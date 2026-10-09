use std::env;
use std::path::PathBuf;

fn main() {
    for key in [
        "ONEUI_MOBILE_LIB_DIR",
        "ONEUI_MOBILE_LINK_KIND",
        "ONEUI_MOBILE_CXX_RUNTIME",
    ] {
        println!("cargo:rerun-if-env-changed={key}");
    }
    let path = PathBuf::from(env::var_os("ONEUI_MOBILE_LIB_DIR")
        .expect("Set ONEUI_MOBILE_LIB_DIR to the precompiled mobile runtime library directory; this crate never builds OneUI"));
    assert!(
        path.is_absolute() && path.is_dir(),
        "ONEUI_MOBILE_LIB_DIR must be an existing absolute directory"
    );
    let kind = env::var("ONEUI_MOBILE_LINK_KIND").unwrap_or_else(|_| "static".into());
    assert!(
        kind == "static" || kind == "dylib",
        "ONEUI_MOBILE_LINK_KIND must be static or dylib"
    );
    println!("cargo:rustc-link-search=native={}", path.display());
    println!("cargo:rustc-link-lib={kind}=oneui_mobile_runtime");
    if kind == "static" {
        let target = env::var("TARGET").expect("Cargo TARGET");
        let cpp = env::var("ONEUI_MOBILE_CXX_RUNTIME").unwrap_or_else(|_| {
            if target.contains("msvc") { String::new() }
            else if target.contains("apple") { "c++".into() }
            else if target.contains("android") || target.contains("ohos") {
                panic!("Set ONEUI_MOBILE_CXX_RUNTIME to match the native SDK C++ runtime (and its linker search path)")
            } else { "stdc++".into() }
        });
        if !cpp.is_empty() {
            println!("cargo:rustc-link-lib={cpp}");
        }
    }
}

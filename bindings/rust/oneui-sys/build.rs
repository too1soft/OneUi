use std::env;
use std::fs;
use std::path::{Path, PathBuf};

fn main() {
    println!("cargo:rerun-if-env-changed=ONEUI_LIB_DIR");
    println!("cargo:rerun-if-env-changed=ONEUI_SKIA_LIB_DIR");
    println!("cargo:rerun-if-env-changed=ONEUI_STATIC_LIBS");

    let lib_dir = env::var_os("ONEUI_LIB_DIR").map(PathBuf::from).unwrap_or_else(|| {
        panic!(
            "ONEUI_LIB_DIR is required and must point to the directory containing oneui.lib / liboneui.so"
        )
    });

    println!("cargo:rustc-link-search=native={}", lib_dir.display());

    if cfg!(feature = "static-link") {
        link_static(&lib_dir);
        return;
    }

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

fn link_static(lib_dir: &Path) {
    println!("cargo:rustc-link-lib=static=oneui_static");
    if let Some(skia_dir) = env::var_os("ONEUI_SKIA_LIB_DIR") {
        println!(
            "cargo:rustc-link-search=native={}",
            PathBuf::from(skia_dir).display()
        );
    }

    let static_libs = env::var("ONEUI_STATIC_LIBS").unwrap_or_else(|_| "skia".to_owned());
    for name in static_libs
        .split([',', ';'])
        .map(str::trim)
        .filter(|name| !name.is_empty())
    {
        println!("cargo:rustc-link-lib=static={name}");
    }

    if env::var("CARGO_CFG_TARGET_OS").as_deref() == Ok("windows") {
        for name in [
            "user32", "gdi32", "shell32", "dwmapi", "imm32", "dwrite", "opengl32", "ole32",
            "oleaut32", "uuid", "fontsub", "usp10",
        ] {
            println!("cargo:rustc-link-lib={name}");
        }
    }

    println!(
        "cargo:rerun-if-changed={}",
        lib_dir.join("oneui_static.lib").display()
    );
}

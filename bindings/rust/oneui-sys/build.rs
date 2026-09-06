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
    let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap_or_default();
    if target_os == "linux" || target_os == "macos" {
        let runtime = if target_os == "macos" {
            "liboneui.dylib"
        } else {
            "liboneui.so"
        };
        println!("cargo:rerun-if-changed={}", lib_dir.join(runtime).display());
        // Applies to this package's tests/examples. Applications should install
        // the SDK beside their binary and set their own relocatable rpath.
        println!("cargo:rustc-link-arg=-Wl,-rpath,{}", lib_dir.display());
    }

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
    println!("cargo:rustc-link-lib=static=oneui_text");
    let skia_dir = env::var_os("ONEUI_SKIA_LIB_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|| lib_dir.to_path_buf());
    println!("cargo:rustc-link-search=native={}", skia_dir.display());

    let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap_or_default();
    let target_env = env::var("CARGO_CFG_TARGET_ENV").unwrap_or_default();
    track_archive(lib_dir, "oneui_static", &target_env);
    track_archive(lib_dir, "oneui_text", &target_env);
    let mut defaults = vec![
        "skparagraph",
        "skshaper",
        "skunicode_icu",
        "skunicode_core",
        "harfbuzz",
        "icu",
        "skia",
        "expat",
        "skcms",
        "zlib",
    ];
    if target_env == "msvc" {
        defaults.extend(["libjpeg", "libjpeg12", "libjpeg16", "libpng", "libwebp"]);
    } else {
        defaults.extend(["jpeg", "jpeg12", "jpeg16", "png", "webp"]);
    }
    if target_os == "linux" {
        defaults.push("freetype2");
    }
    if matches!(
        env::var("CARGO_CFG_TARGET_ARCH").as_deref(),
        Ok("x86" | "x86_64")
    ) {
        defaults.push(if target_env == "msvc" {
            "libwebp_sse41"
        } else {
            "webp_sse41"
        });
    }
    let static_libs = env::var("ONEUI_STATIC_LIBS").unwrap_or_else(|_| defaults.join(","));
    for name in static_libs
        .split([',', ';'])
        .map(str::trim)
        .filter(|name| !name.is_empty())
    {
        println!("cargo:rustc-link-lib=static={name}");
        track_archive(&skia_dir, name, &target_env);
    }

    if env::var("CARGO_CFG_TARGET_OS").as_deref() == Ok("windows") {
        for name in [
            "user32",
            "gdi32",
            "shell32",
            "dwmapi",
            "imm32",
            "dwrite",
            "opengl32",
            "ole32",
            "oleaut32",
            "uuid",
            "fontsub",
            "usp10",
            "windowscodecs",
            "advapi32",
        ] {
            println!("cargo:rustc-link-lib={name}");
        }
    }

    match env::var("CARGO_CFG_TARGET_OS").as_deref() {
        Ok("linux") => {
            for name in [
                "stdc++",
                "X11",
                "Xrandr",
                "wayland-client",
                "wayland-cursor",
                "xkbcommon",
                "fontconfig",
                "dl",
                "pthread",
                "m",
            ] {
                println!("cargo:rustc-link-lib={name}");
            }
        }
        Ok("macos") => {
            println!("cargo:rustc-link-lib=c++");
            for name in ["Cocoa", "CoreText", "CoreGraphics", "CoreFoundation"] {
                println!("cargo:rustc-link-lib=framework={name}");
            }
        }
        _ => {}
    }
}

// Rebuilding a private text dependency must relink consumers even when the
// OneUI archive itself is unchanged (e.g. an ICU conformance patch).
fn track_archive(directory: &Path, name: &str, target_env: &str) {
    let filename = if target_env == "msvc" {
        format!("{name}.lib")
    } else {
        format!("lib{name}.a")
    };
    println!(
        "cargo:rerun-if-changed={}",
        directory.join(filename).display()
    );
}

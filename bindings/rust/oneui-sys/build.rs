use sha2::{Digest, Sha256};
use std::env;
use std::fs;
use std::path::{Path, PathBuf};

fn main() {
    println!("cargo:rerun-if-env-changed=ONEUI_SDK_DIR");
    if let Some(sdk) = env::var_os("ONEUI_SDK_DIR").map(PathBuf::from) {
        if cfg!(feature = "static-link") {
            link_sdk(&sdk);
            return;
        }
    }
    if env::var("TARGET").unwrap_or_default().contains("-win7-") && cfg!(feature = "static-link") {
        panic!("Win7 static consumers require a locked ONEUI_SDK_DIR; guessed archive lists are not allowed");
    }
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

fn link_sdk(sdk: &Path) {
    let target = env::var("TARGET").unwrap_or_default();
    assert!(
        target.ends_with("windows-msvc"),
        "This SDK is an MSVC Windows SDK"
    );
    println!("cargo:rerun-if-env-changed=ONEUI_SDK_MANIFEST_SHA256");
    let manifest_path = sdk.join("sdk-manifest.json");
    let bytes = fs::read(&manifest_path).expect("SDK manifest missing");
    let expected = env::var("ONEUI_SDK_MANIFEST_SHA256")
        .expect("Pass the SDK manifest hash from compat.lock.json");
    assert_eq!(
        format!("{:x}", Sha256::digest(&bytes)),
        expected,
        "SDK manifest differs from locked input"
    );
    let manifest: serde_json::Value = serde_json::from_slice(&bytes).expect("Invalid SDK manifest");
    let arch = env::var("CARGO_CFG_TARGET_ARCH").unwrap_or_default();
    assert_eq!(
        manifest["architecture"].as_str(),
        Some(arch.as_str()),
        "SDK architecture mismatch"
    );
    assert_eq!(
        manifest["configuration"].as_str(),
        Some("Release"),
        "SDK must be Release"
    );
    assert_eq!(
        manifest["crt"].as_str(),
        Some("static"),
        "SDK must use static CRT"
    );
    if target.contains("-win7-") {
        assert_eq!(
            manifest["profile"].as_str(),
            Some("legacy"),
            "Win7 requires legacy SDK"
        );
    }
    let root = sdk.canonicalize().expect("SDK root missing");
    for entry in manifest["files"]
        .as_array()
        .expect("SDK file inventory missing")
    {
        let name = entry["path"].as_str().expect("SDK file name missing");
        assert!(
            !name.contains('\\')
                && !name.contains(':')
                && !name.starts_with('/')
                && name
                    .split('/')
                    .all(|p| !p.is_empty() && p != "." && p != ".."),
            "Unsafe SDK path"
        );
        let path = root.join(name).canonicalize().expect("SDK file missing");
        assert!(path.starts_with(&root), "SDK file escapes root");
        let content = fs::read(&path).expect("Cannot read SDK input");
        assert_eq!(
            entry["size"].as_u64(),
            Some(content.len() as u64),
            "SDK size mismatch"
        );
        assert_eq!(
            entry["sha256"].as_str(),
            Some(format!("{:x}", Sha256::digest(&content)).as_str()),
            "SDK hash mismatch"
        );
        println!("cargo:rerun-if-changed={}", path.display());
    }
    let lib = sdk.join("lib");
    println!("cargo:rustc-link-search=native={}", lib.display());
    println!(
        "cargo:rerun-if-changed={}",
        sdk.join("sdk-manifest.json").display()
    );
    for (file, kind) in [
        ("static-libraries.txt", "static="),
        ("system-libraries.txt", ""),
    ] {
        let path = sdk.join(file);
        println!("cargo:rerun-if-changed={}", path.display());
        let text = fs::read_to_string(&path).expect("SDK link closure is missing");
        assert!(!text.trim().is_empty(), "SDK link closure is empty");
        for name in text.lines().map(str::trim).filter(|s| !s.is_empty()) {
            assert!(
                name.bytes().all(|b| b.is_ascii_alphanumeric() || b == b'_'),
                "Invalid library name in SDK"
            );
            if !kind.is_empty() {
                assert!(
                    lib.join(format!("{name}.lib")).is_file(),
                    "SDK archive missing: {name}"
                );
                track_archive(&lib, name, "msvc");
            }
            println!("cargo:rustc-link-lib={kind}{name}");
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
            "bcrypt",
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

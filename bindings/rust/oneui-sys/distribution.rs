//! Versioned distribution SDKs. Legacy consumer manifests keep their existing path.
use sha2::{Digest, Sha256};
use std::{env, fs, path::{Component, Path}};

pub fn link(sdk: &Path) -> bool {
    let manifest_path = sdk.join("sdk-manifest.json");
    let Ok(bytes) = fs::read(&manifest_path) else { return false };
    let manifest: serde_json::Value = serde_json::from_slice(&bytes).expect("Invalid SDK manifest");
    if manifest["distributionVersion"].as_u64() != Some(1) { return false; }
    println!("cargo:rerun-if-changed={}", manifest_path.display());
    println!("cargo:rerun-if-env-changed=ONEUI_SDK_MANIFEST_SHA256");
    if let Ok(expected) = env::var("ONEUI_SDK_MANIFEST_SHA256") {
        assert_eq!(format!("{:x}", Sha256::digest(&bytes)), expected, "SDK manifest differs from project lock");
    }
    let os = env::var("CARGO_CFG_TARGET_OS").unwrap_or_default();
    let arch = env::var("CARGO_CFG_TARGET_ARCH").unwrap_or_default();
    let expected_arch = if arch == "aarch64" { "arm64" } else { &arch };
    assert_eq!(manifest["platform"].as_str(), Some(os.as_str()), "SDK platform mismatch");
    assert_eq!(manifest["architecture"].as_str(), Some(expected_arch), "SDK architecture mismatch");
    let mode = if cfg!(feature="static-link") { "static" } else { "shared" };
    assert_eq!(manifest["linkage"].as_str(), Some(mode), "Download the SDK for the requested linkage");
    let target = env::var("TARGET").unwrap_or_default();
    if os == "windows" {
        assert!(target.ends_with("windows-msvc"), "This SDK requires the MSVC target");
        if target.contains("-win7-") {
            assert_eq!(manifest["profile"].as_str(), Some("legacy"), "Win7 requires legacy SDK");
        }
        if mode == "static" {
            assert!(env::var("CARGO_CFG_TARGET_FEATURE").unwrap_or_default().split(',').any(|f| f=="crt-static"), "Static SDK requires target-feature=+crt-static");
        }
    }
    let root = sdk.canonicalize().expect("SDK directory missing");
    for file in manifest["files"].as_array().expect("SDK inventory missing") {
        let name = file["path"].as_str().expect("SDK path missing");
        assert!(!name.contains('\\') && !name.contains(':') && Path::new(name).components().all(|c| matches!(c,Component::Normal(_))), "Unsafe SDK path");
        let path = root.join(name).canonicalize().expect("SDK file missing");
        assert!(path.starts_with(&root), "SDK path escapes root");
        let content = fs::read(&path).expect("Cannot read SDK file");
        assert_eq!(Some(content.len() as u64), file["size"].as_u64(), "SDK size mismatch: {name}");
        assert_eq!(Some(format!("{:x}",Sha256::digest(&content)).as_str()), file["sha256"].as_str(), "SDK hash mismatch: {name}");
        println!("cargo:rerun-if-changed={}",path.display());
    }
    println!("cargo:rustc-link-search=native={}",root.join("lib").display());
    if mode == "shared" {
        println!("cargo:rustc-link-lib=dylib=oneui");
    } else {
        for library in manifest["staticLibraries"].as_array().expect("Static closure missing") {
            println!("cargo:rustc-link-lib=static={}",library.as_str().expect("Invalid archive name"));
        }
        for library in manifest["systemLibraries"].as_array().expect("System closure missing") {
            println!("cargo:rustc-link-lib={}",library.as_str().expect("Invalid system library"));
        }
        if let Some(frameworks) = manifest["frameworks"].as_array() {
            for framework in frameworks { println!("cargo:rustc-link-lib=framework={}",framework.as_str().expect("Invalid framework")); }
        }
    }
    if os == "linux" || os == "macos" {
        println!("cargo:rustc-link-arg=-Wl,-rpath,{}",root.join("lib").display());
    }
    true
}

use std::path::PathBuf;
#[path = "build_support/signatures.rs"]
mod signatures;

fn c_enum(name: &str) -> bool {
    matches!(
        name,
        "OneUiErrorCode"
            | "OneUiFileDialogMode"
            | "OneUiPointerButton"
            | "OneUiTerminalPointerAction"
            | "OneUiCommandResult"
            | "OneUiStackDirection"
            | "OneUiStackAlign"
            | "OneUiSplitOrientation"
            | "OneUiVideoScaleMode"
            | "OneUiPixelFormat"
            | "OneUiRemoteCursorMode"
            | "OneUiButtonVariant"
            | "OneUiTerminalCursorStyle"
            | "OneUiTerminalAuxiliaryButtonAction"
    )
}

fn main() {
    let root = PathBuf::from(std::env::var_os("CARGO_MANIFEST_DIR").unwrap()).join("../../..");
    println!(
        "cargo:rerun-if-changed={}",
        root.join("include/oneui/oneui_c_api.h").display()
    );
    println!("cargo:rerun-if-changed=../oneui-sys/src");
    println!("cargo:rerun-if-env-changed=ONEUI_ABI_BINDINGS");
    println!("cargo:rerun-if-changed=../oneui-sys/supported-symbols.txt");
    let source = std::env::var_os("ONEUI_ABI_BINDINGS")
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from("../oneui-sys/src/lib.rs"));
    println!("cargo:rerun-if-changed={}", source.display());
    let output = PathBuf::from(std::env::var_os("OUT_DIR").unwrap());
    std::fs::write(
        output.join("bindings.rs"),
        format!(
            "#[path = {:?}] mod declarations;\npub use declarations::*;\n",
            source.canonicalize().unwrap().to_string_lossy(),
        ),
    )
    .unwrap();
    let target = std::env::var("TARGET").unwrap();
    let mut generator = ctest::TestGenerator::new();
    generator
        .header("oneui/oneui_c_api.h")
        .include(root.join("include"))
        .skip_private(true)
        // On Windows, rustc may take an import thunk's address while C uses
        // the DLL entry from its IAT. Typed compiler checks below cover all
        // signatures; exact pointer identity is only meaningful elsewhere.
        .skip_fn_ptrcheck(move |_| target.contains("windows"))
        .alias_is_c_enum(c_enum)
        // C may choose a signed or unsigned enum representation. Rust's
        // integer aliases preserve all bits; size/alignment/roundtrip stay checked.
        .skip_signededness(c_enum)
        // These are deliberately opaque handles, never passed by value.
        .skip_struct(|item| {
            matches!(
                item.ident(),
                "OneUiWindow"
                    | "OneUiWidget"
                    | "OneUiFocusBookmark"
                    | "OneUiStyleSheet"
                    | "OneUiCommandRegistration"
                    | "OneUiTray"
                    | "OneUiApplication"
                    | "OneUiCloseDecision"
                    | "OneUiWindowSubscription"
                    | "OneUiModalSession"
                    | "OneUiNotificationService"
                    | "OneUiDockModel"
                    | "OneUiDockSession"
            )
        })
        .rename_type(|name| match name {
            "WChar" => Some("wchar_t".into()),
            _ => None,
        })
        .rename_alias(|item| (item.ident() == "WChar").then(|| "wchar_t".into()))
        .rename_constant(|item| match item.ident() {
            "UTF8_ABI_VERSION" => Some("ONEUI_UTF8_ABI_VERSION".into()),
            "FILE_DIALOG_OPEN_FILE" => Some("OneUiFileDialogOpenFile".into()),
            "FILE_DIALOG_SAVE_FILE" => Some("OneUiFileDialogSaveFile".into()),
            "FILE_DIALOG_SELECT_FOLDER" => Some("OneUiFileDialogSelectFolder".into()),
            "WINDOW_CAPABILITY_CLIPBOARD" => Some("OneUiWindowCapabilityClipboard".into()),
            "WINDOW_CAPABILITY_IME" => Some("OneUiWindowCapabilityIme".into()),
            "WINDOW_CAPABILITY_PLACEMENT" => Some("OneUiWindowCapabilityPlacement".into()),
            "WINDOW_CAPABILITY_ACTIVATION" => Some("OneUiWindowCapabilityActivation".into()),
            "WINDOW_CAPABILITY_TOPMOST" => Some("OneUiWindowCapabilityTopmost".into()),
            "WINDOW_CAPABILITY_TRAY" => Some("OneUiWindowCapabilityTray".into()),
            "WINDOW_CAPABILITY_NATIVE_DIALOGS" => Some("OneUiWindowCapabilityNativeDialogs".into()),
            "NATIVE_MENU_ENABLED" => Some("OneUiNativeMenuEnabled".into()),
            "NATIVE_MENU_SEPARATOR" => Some("OneUiNativeMenuSeparator".into()),
            "NATIVE_MENU_CHECK" => Some("OneUiNativeMenuCheck".into()),
            "NATIVE_MENU_RADIO" => Some("OneUiNativeMenuRadio".into()),
            "NATIVE_MENU_CHECKED" => Some("OneUiNativeMenuChecked".into()),
            "WINDOW_CAPABILITY_MENU_BAR" => Some("OneUiWindowCapabilityMenuBar".into()),
            "WINDOW_CAPABILITY_TRAY_TOOLTIP" => Some("OneUiWindowCapabilityTrayTooltip".into()),
            "WINDOW_CAPABILITY_FILE_DROP" => Some("OneUiWindowCapabilityFileDrop".into()),
            _ => None,
        });
    if std::env::var("CARGO_CFG_TARGET_ENV").as_deref() == Ok("msvc") {
        generator.flag("/utf-8");
    }
    ctest::generate_test(&mut generator, &source, "abi.rs")
        .expect("C/Rust ABI generation and compiler checks must succeed");
    let signature_file = PathBuf::from(std::env::var_os("OUT_DIR").unwrap()).join("signatures.c");
    let count = signatures::generate(
        &source,
        &signature_file,
        &root.join("bindings/rust/oneui-sys/supported-symbols.txt"),
        &std::env::var("TARGET").unwrap(),
    );
    let mut compiler = cc::Build::new();
    compiler
        .file(signature_file)
        .include(root.join("include"))
        .warnings_into_errors(true);
    if std::env::var("CARGO_CFG_TARGET_ENV").as_deref() == Ok("msvc") {
        compiler.flag("/utf-8");
    }
    compiler.compile("oneui_signatures");
    println!("cargo:rustc-env=ONEUI_ABI_SIGNATURE_COUNT={count}");
}

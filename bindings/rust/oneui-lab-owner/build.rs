fn main() {
    match std::env::var("CARGO_CFG_TARGET_OS").as_deref() {
        Ok("linux") => println!("cargo:rustc-cdylib-link-arg=-Wl,-soname,liboneui_lab_owner.so"),
        Ok("macos") => println!(
            "cargo:rustc-cdylib-link-arg=-Wl,-install_name,@rpath/liboneui_lab_owner.dylib"
        ),
        _ => {}
    }
}

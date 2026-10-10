use std::{fs, path::PathBuf, process::Command};

struct Fixture(PathBuf);
impl Drop for Fixture {
    fn drop(&mut self) {
        if let Err(error) = fs::remove_dir_all(&self.0) {
            eprintln!(
                "compile-fail fixture cleanup failed: {}: {error}",
                self.0.display()
            );
        }
    }
}
pub fn run() {
    let root = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("../../..")
        .canonicalize()
        .unwrap();
    let parent = root.join("build/cargo-target/viewc-tests");
    fs::create_dir_all(&parent).unwrap();
    let path = parent.join(format!("compile-fail-{}", std::process::id()));
    fs::create_dir(&path).unwrap();
    let fixture = Fixture(path.canonicalize().unwrap());
    assert!(fixture.0.starts_with(parent.canonicalize().unwrap()));
    let deps = std::env::current_exe()
        .unwrap()
        .parent()
        .unwrap()
        .to_owned();
    let mut libraries: Vec<_> = fs::read_dir(&deps)
        .unwrap()
        .filter_map(Result::ok)
        .map(|e| e.path())
        .filter(|p| {
            let n = p.file_name().unwrap().to_string_lossy();
            (n.starts_with("liboneui-") || n.starts_with("oneui-")) && n.ends_with(".rlib")
        })
        .collect();
    libraries.sort_by_key(|p| fs::metadata(p).unwrap().modified().unwrap());
    let library = libraries.last().expect("Cargo's oneui rlib");
    let model = root.join("bindings/rust/oneui-view-tests/tests/model.rs");
    let prefix = format!(
        "#[path = {:?}] mod model;\nuse model::page::RustPageVm;\n",
        model.to_str().unwrap()
    );
    let cases = [
        ("positive", "fn check(vm: &mut model::Vm, cx: &oneui::view::VmContext) { vm.set_title(String::new(), cx).unwrap(); let _ = model::page::RustPageCommand::Choose { target: String::new() }; }", None),
        ("wrong_type", "fn check(vm: &mut model::Vm, cx: &oneui::view::VmContext) { vm.set_enabled(42, cx).unwrap(); }", Some("E0308")),
        ("readonly", "fn check(vm: &mut model::Vm, cx: &oneui::view::VmContext) { vm.set_status(String::new(), cx).unwrap(); }", Some("E0599")),
        ("command_argument", "fn check() { let _ = model::page::RustPageCommand::Choose { target: false }; }", Some("E0308")),
        ("view_send", "fn require<T: Send>() {} fn check() { require::<oneui::view::View>(); }", Some("E0277")),
        ("notifier_sync", "fn require<T: Sync>() {} fn check() { require::<oneui::view::ViewNotifier>(); }", Some("E0277")),
    ];
    for (name, body, error) in cases {
        let input = fixture.0.join(format!("{name}.rs"));
        fs::write(&input, format!("{prefix}{body}\n")).unwrap();
        let output = Command::new(std::env::var_os("RUSTC").unwrap_or_else(|| "rustc".into()))
            .arg("--edition=2021")
            .arg("--crate-type=lib")
            .arg("--emit=metadata")
            .arg("--extern")
            .arg(format!("oneui={}", library.display()))
            .arg("-L")
            .arg(format!("dependency={}", deps.display()))
            .arg("--out-dir")
            .arg(&fixture.0)
            .arg(&input)
            .env("OUT_DIR", env!("OUT_DIR"))
            .output()
            .unwrap();
        let stderr = String::from_utf8_lossy(&output.stderr);
        match error {
            None => assert!(output.status.success(), "positive fixture failed: {stderr}"),
            Some(code) => {
                assert!(!output.status.success(), "{name} unexpectedly compiled");
                assert!(stderr.contains(code), "{name}: expected {code}: {stderr}");
            }
        }
        println!("PASS compiler fixture {name}");
    }
    let path = fixture.0.clone();
    drop(fixture);
    assert!(!path.exists(), "temporary fixture must be removed");
}

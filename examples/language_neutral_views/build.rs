use std::{env, path::PathBuf, process::Command};
fn main() {
    let root = PathBuf::from(env::var_os("CARGO_MANIFEST_DIR").unwrap());
    let viewc = env::var_os("ONEUI_VIEWC")
        .map(PathBuf::from)
        .unwrap_or_else(|| {
            root.join("../../build/capture").join(if cfg!(windows) {
                "oneui-viewc.exe"
            } else {
                "oneui-viewc"
            })
        });
    let out = PathBuf::from(env::var_os("OUT_DIR").unwrap());
    println!("cargo:rerun-if-env-changed=ONEUI_VIEWC");
    println!("cargo:rerun-if-changed={}", viewc.display());
    for extension in ["one", "oneschema"] {
        println!("cargo:rerun-if-changed=SettingsBasic.{extension}");
    }
    assert!(Command::new(&viewc)
        .args(["--emit", "rust", "--input"])
        .arg(root.join("SettingsBasic.one"))
        .arg("--schema")
        .arg(root.join("SettingsBasic.oneschema"))
        .arg("--output")
        .arg(out.join("settings.rs"))
        .arg("--ir-output")
        .arg(out.join("SettingsBasic.oneview"))
        .status()
        .expect("run host viewc")
        .success());
    for name in ["SettingsProbe", "TableProbe"] {
        println!("cargo:rerun-if-changed=bench/{name}.one");
        let schema = if name == "SettingsProbe" {
            root.join("SettingsBasic.oneschema")
        } else {
            root.join("bench/TableProbe.oneschema")
        };
        println!("cargo:rerun-if-changed={}", schema.display());
        assert!(Command::new(&viewc)
            .args([
                "--emit",
                if name == "SettingsProbe" {
                    "ir"
                } else {
                    "rust"
                },
                "--input"
            ])
            .arg(root.join(format!("bench/{name}.one")))
            .arg("--schema")
            .arg(schema)
            .arg("--output")
            .arg(out.join(if name == "SettingsProbe" {
                "SettingsProbe.oneview"
            } else {
                "table_probe.rs"
            }))
            .status()
            .expect("compile benchmark IR")
            .success());
    }
}

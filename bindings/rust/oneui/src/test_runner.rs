//! Minimal process-main-thread harness for native UI tests (including AppKit).

use std::panic::{catch_unwind, AssertUnwindSafe};
use std::process::ExitCode;

pub(super) fn run(tests: &[(&str, fn())]) -> ExitCode {
    let mut filters = Vec::new();
    let mut skips = Vec::new();
    let mut exact = false;
    let mut list = false;
    let mut args = std::env::args().skip(1);
    while let Some(arg) = args.next() {
        match arg.as_str() {
            "--list" => list = true,
            "--exact" => exact = true,
            "--nocapture" | "--show-output" => {}
            "--skip" => match args.next() {
                Some(pattern) => skips.push(pattern),
                None => return argument_error("--skip requires a pattern"),
            },
            "--test-threads" => {
                if args.next().as_deref() != Some("1") {
                    return argument_error("native UI tests require --test-threads 1");
                }
            }
            "--test-threads=1" => {}
            "--help" | "-h" => {
                println!("Native UI tests run on the process main thread.\nOptions: [FILTER ...] --exact --skip FILTER --list --nocapture --test-threads 1");
                return ExitCode::SUCCESS;
            }
            value if value.starts_with('-') => return argument_error(value),
            _ => filters.push(arg),
        }
    }
    let matches = |name: &str, pattern: &str| {
        if exact {
            name == pattern
        } else {
            name.contains(pattern)
        }
    };
    let selected: Vec<_> = tests
        .iter()
        .filter(|(name, _)| {
            (filters.is_empty() || filters.iter().any(|f| matches(name, f)))
                && !skips.iter().any(|f| matches(name, f))
        })
        .collect();
    if list {
        for (name, _) in &selected {
            println!("{name}: test");
        }
        println!("\n{} tests", selected.len());
        return ExitCode::SUCCESS;
    }
    println!(
        "running {} tests on the process main thread",
        selected.len()
    );
    let mut failed = 0;
    for (name, test) in &selected {
        let ok = catch_unwind(AssertUnwindSafe(test)).is_ok();
        println!("test {name} ... {}", if ok { "ok" } else { "FAILED" });
        if !ok {
            failed += 1;
        }
    }
    println!(
        "\ntest result: {}. {} passed; {failed} failed; 0 ignored; {} filtered out",
        if failed == 0 { "ok" } else { "FAILED" },
        selected.len() - failed,
        tests.len() - selected.len()
    );
    if failed == 0 {
        ExitCode::SUCCESS
    } else {
        ExitCode::FAILURE
    }
}

fn argument_error(message: &str) -> ExitCode {
    eprintln!("Unsupported native test argument: {message}");
    ExitCode::from(2)
}

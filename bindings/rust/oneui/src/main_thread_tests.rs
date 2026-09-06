// Compile the same implementation and private tests in a main-thread harness.
// A separate entry avoids declaring lib.rs as two Cargo target paths.
#[path = "lib.rs"]
mod implementation;
pub use implementation::*;

fn main() -> std::process::ExitCode {
    implementation::run_native_tests()
}

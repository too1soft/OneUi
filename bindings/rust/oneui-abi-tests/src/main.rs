//! Developer-only checks against the actual public C header and linked library.
#![allow(non_snake_case, non_camel_case_types, dead_code)]

// Compile the same source used by the generator, including isolated negative
// fixtures. The dependency still supplies the actual product runtime linkage.
include!(concat!(env!("OUT_DIR"), "/bindings.rs"));

mod layout {
    use crate::*;
    include!(concat!(env!("OUT_DIR"), "/abi.rs"));
    pub fn run() {
        main();
    }
}

fn main() {
    assert_eq!(UTF8_ABI_VERSION, oneui_sys::UTF8_ABI_VERSION);
    // SAFETY: the version query takes no pointers and has no UI-thread state.
    let runtime_version = unsafe { oneui_sys::oneui_utf8_abi_version() };
    assert_eq!(
        UTF8_ABI_VERSION, runtime_version,
        "linked OneUI ABI version"
    );
    layout::run();
    extern "C" {
        fn oneui_abi_checked_signatures() -> usize;
    }
    // SAFETY: this test-owned C helper takes no arguments and returns size_t.
    let checked = unsafe { oneui_abi_checked_signatures() };
    assert_eq!(
        checked,
        env!("ONEUI_ABI_SIGNATURE_COUNT").parse::<usize>().unwrap()
    );
    println!("PASSED {checked} compiler-checked function signatures");
}

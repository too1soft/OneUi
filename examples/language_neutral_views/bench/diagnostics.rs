use std::{fs::OpenOptions, io::Write, path::Path};
// Test-only access to an existing C++ export. The benchmark is Windows/MSVC
// x64 only; this is deliberately not a promised SDK C ABI or product VM glue.
#[link(name = "oneui")]
extern "C" {
    #[link_name = "?setMotionEnabled@oneui@@YAX_N@Z"]
    fn set_motion_enabled(enabled: bool);
}
pub fn disable_motion() {
    unsafe {
        set_motion_enabled(false);
    }
}
pub fn reset() {
    #[cfg(feature = "bench-diagnostics")]
    oneui::view::diagnostics::reset();
}
pub fn save(path: &Path, name: &str, case: &str) {
    #[cfg(feature = "bench-diagnostics")]
    {
        let mut out = OpenOptions::new()
            .create(true)
            .append(true)
            .open(path.join("callback-counters.txt"))
            .unwrap();
        writeln!(
            out,
            "{name},{case},{:?}",
            oneui::view::diagnostics::snapshot()
        )
        .unwrap();
    }
    #[cfg(not(feature = "bench-diagnostics"))]
    let _ = (path, name, case);
}

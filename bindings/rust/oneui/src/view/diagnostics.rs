//! Optional, UI-thread callback counters for untimed benchmark diagnostics.
//! Excluded from normal builds; these count VM FFI, not native widget setters.
use std::cell::Cell;
#[derive(Clone, Copy, Debug, Default)]
pub struct Snapshot {
    pub callbacks: u64,
    pub gets: u64,
    pub sets: u64,
    pub invokes: u64,
    pub list_info: u64,
    pub list_keys: u64,
    pub list_acquires: u64,
    pub notifies: u64,
    pub live_connections: u64,
    pub peak_connections: u64,
}
thread_local! { static COUNTS:Cell<Snapshot>=Cell::new(Snapshot::default()); }
pub(crate) fn record(action: impl FnOnce(&mut Snapshot)) {
    COUNTS.with(|cell| {
        let mut value = cell.get();
        action(&mut value);
        cell.set(value);
    });
}
pub fn snapshot() -> Snapshot {
    COUNTS.with(Cell::get)
}
/// Call only before constructing a diagnostic workload, after all previous
/// views disconnected. This deliberately does not count Rust Rc clones.
pub fn reset() {
    assert_eq!(snapshot().live_connections, 0);
    COUNTS.with(|c| c.set(Snapshot::default()));
}

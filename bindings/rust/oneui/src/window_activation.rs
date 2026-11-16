//! Window activation shared by an owned window and its lifetime-checked dispatcher.
use super::{UiDispatcher, Window, sys};
impl Window {
    pub fn activate(&self) {
        let _ = self.dispatcher().activate();
    }
}
impl UiDispatcher {
    /// Request activation on the owning UI thread. False means wrong thread or
    /// a closed window; the operating system decides whether focus is granted.
    pub fn activate(&self) -> bool {
        if std::thread::current().id() != self.state.ui_thread {
            return false;
        }
        self.state
            .with_raw(|raw| unsafe { sys::oneui_window_activate(raw) })
            .is_some()
    }
}

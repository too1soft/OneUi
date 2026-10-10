//! Input for opt-in renderer QA builds; never sends input to the operating system.
use crate::{sys, Widget};
unsafe extern "C" {
    fn oneui_widget_test_freeze_caret(root: *mut sys::OneUiWidget, enabled: i32) -> i32;
    fn oneui_widget_test_click(root: *mut sys::OneUiWidget, x: f32, y: f32) -> i32;
    fn oneui_widget_test_wheel(root: *mut sys::OneUiWidget, x: f32, y: f32, delta: f32) -> i32;
    fn oneui_widget_test_text_utf8(
        root: *mut sys::OneUiWidget,
        text: sys::OneUiUtf8String,
        replace: i32,
    ) -> i32;
}
impl Widget {
    /// Freeze input carets on this UI thread in an opt-in capture build.
    #[doc(hidden)]
    pub fn test_freeze_caret(&self, enabled: bool) -> bool {
        unsafe { oneui_widget_test_freeze_caret(self.raw.as_ptr(), i32::from(enabled)) != 0 }
    }
    /// Send a click through this widget tree in a test-frame-capture build.
    #[doc(hidden)]
    pub fn test_click(&self, x: f32, y: f32) -> bool {
        unsafe { oneui_widget_test_click(self.raw.as_ptr(), x, y) != 0 }
    }
    /// Send wheel input through this widget tree in a test-frame-capture build.
    #[doc(hidden)]
    pub fn test_wheel(&self, x: f32, y: f32, delta: f32) -> bool {
        unsafe { oneui_widget_test_wheel(self.raw.as_ptr(), x, y, delta) != 0 }
    }
    /// Commit text to the focused control; optionally select all first.
    #[doc(hidden)]
    pub fn test_text(&self, text: &str, replace: bool) -> bool {
        unsafe {
            oneui_widget_test_text_utf8(
                self.raw.as_ptr(),
                sys::OneUiUtf8String::from_str(text),
                i32::from(replace),
            ) != 0
        }
    }
}

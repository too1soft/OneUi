//! UTF-8 operating system clipboard access.

use super::sys;

/// Writes UTF-8 text to the operating system clipboard.
pub fn set_clipboard_text(text: &str) -> bool {
    let text = sys::OneUiUtf8String::from_str(text);
    unsafe { sys::oneui_clipboard_set_text_utf8(text) != 0 }
}

/// Reads UTF-8 text from the operating system clipboard.
pub fn clipboard_text() -> Option<String> {
    clipboard_text_with_limit(16 * 1024 * 1024)
}

/// Reads text only when it fits the caller's byte budget, including races with
/// another clipboard owner. Native access still belongs on the UI thread.
pub fn clipboard_text_with_limit(max_bytes: usize) -> Option<String> {
    let required = unsafe { sys::oneui_clipboard_get_text_utf8(std::ptr::null_mut(), 0) };
    if required == 0 || required > max_bytes.saturating_add(1) {
        return None;
    }

    let mut buffer = vec![0u8; required];
    let written =
        unsafe { sys::oneui_clipboard_get_text_utf8(buffer.as_mut_ptr().cast(), buffer.len()) };
    if written == 0 || written > buffer.len() {
        return None;
    }
    buffer.truncate(written);
    if buffer.last() == Some(&0) {
        buffer.pop();
    }
    String::from_utf8(buffer).ok()
}

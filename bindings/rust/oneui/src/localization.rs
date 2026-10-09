use std::ffi::CString;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[repr(i32)]
pub enum BuiltinText {
    MinimizeWindow,
    MaximizeWindow,
    RestoreWindow,
    CloseWindow,
    Accept,
    Cancel,
    ShowMainWindow,
    QuitApplication,
    Loading,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum LocalizationError {
    InteriorNul,
    Rejected,
}

/// Overrides framework text with the application's language. `None` restores
/// the system fallback. Existing accessibility queries see changes immediately;
/// already-open native dialogs keep their labels until the next opening.
pub fn set_builtin_text(key: BuiltinText, text: Option<&str>) -> Result<(), LocalizationError> {
    let text = text
        .map(CString::new)
        .transpose()
        .map_err(|_| LocalizationError::InteriorNul)?;
    // SAFETY: the optional C string lives through the synchronous call. OneUI
    // copies it and serializes access; no GUI handle or borrowed buffer escapes.
    let result = unsafe {
        crate::sys::oneui_set_builtin_text_utf8(
            key as i32,
            text.as_ref().map_or(std::ptr::null(), |s| s.as_ptr()),
        )
    };
    if result == 0 {
        Err(LocalizationError::Rejected)
    } else {
        Ok(())
    }
}

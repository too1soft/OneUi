use std::ffi::CStr;

/// Category reported by OneUI's per-thread native diagnostic.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum NativeErrorCode {
    InvalidArgument,
    OutOfMemory,
    OperationFailed,
    UnknownException,
    /// Preserves a category introduced by a newer native runtime.
    Other(i32),
}

/// Owned snapshot; unlike the C ABI's borrowed message, it can outlive the next call.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct NativeError {
    pub code: NativeErrorCode,
    pub message: String,
}

/// Copies the current thread's last recorded native failure without clearing it.
///
/// Clear before the operation you want to diagnose, check that operation's return
/// value, then read immediately on the same thread. Success leaves the diagnostic
/// unchanged. Some legacy failure paths do not record a diagnostic, so `None`
/// is not evidence of success. Existing `Error` variants remain unchanged.
pub fn last_native_error() -> Option<NativeError> {
    // SAFETY: These queries take no pointers, invoke no callbacks, and do not
    // modify TLS. The native message is non-null and NUL-terminated, and remains
    // valid on this thread until the next error/clear. Copy it before returning.
    unsafe {
        let code = match crate::sys::oneui_last_error_code() {
            0 => return None,
            1 => NativeErrorCode::InvalidArgument,
            2 => NativeErrorCode::OutOfMemory,
            3 => NativeErrorCode::OperationFailed,
            4 => NativeErrorCode::UnknownException,
            other => NativeErrorCode::Other(other),
        };
        let message = CStr::from_ptr(crate::sys::oneui_last_error())
            .to_string_lossy()
            .into_owned();
        Some(NativeError { code, message })
    }
}

/// Clears only this thread's diagnostic. It does not affect widget state.
pub fn clear_native_error() {
    // SAFETY: This no-argument operation only resets the calling thread's TLS.
    unsafe { crate::sys::oneui_clear_error() }
}

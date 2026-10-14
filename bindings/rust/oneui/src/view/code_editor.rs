use super::{check, View, VmResult};
use crate::sys;

/// Primary cursor and document size of a declarative CodeEditor.
/// Offsets are UTF-8 byte offsets; line and column are 1-based.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct CodeEditorState {
    pub revision: u64,
    pub caret: usize,
    pub anchor: usize,
    pub line: usize,
    pub column: usize,
    pub length: usize,
    pub line_count: usize,
    pub cursors: usize,
}

impl View {
    /// Cursor, selection and revision of the uniquely named CodeEditor.
    pub fn code_editor_state(&self, name: &str) -> VmResult<CodeEditorState> {
        let mut value: sys::OneUiViewCodeEditorState = unsafe { std::mem::zeroed() };
        value.struct_size = std::mem::size_of_val(&value) as u32;
        check(unsafe { sys::oneui_view_code_editor_state(self.raw.as_ptr(),
            sys::OneUiUtf8String::from_str(name), &mut value) })?;
        Ok(CodeEditorState {
            revision: value.revision, caret: value.caret as usize, anchor: value.anchor as usize,
            line: value.line as usize, column: value.column as usize, length: value.length as usize,
            line_count: value.line_count as usize, cursors: value.cursors as usize,
        })
    }

    /// The text of a byte range (clamped); `0..usize::MAX` reads everything.
    pub fn code_editor_text(&self, name: &str, range: std::ops::Range<usize>) -> VmResult<String> {
        let raw = sys::OneUiUtf8String::from_str(name);
        let mut length = 0usize;
        check(unsafe { sys::oneui_view_code_editor_text(self.raw.as_ptr(), raw, range.start as u64,
            range.end as u64, std::ptr::null_mut(), 0, &mut length) })?;
        let mut bytes = vec![0u8; length];
        let mut written = 0usize;
        check(unsafe { sys::oneui_view_code_editor_text(self.raw.as_ptr(), raw, range.start as u64,
            range.end as u64, bytes.as_mut_ptr().cast(), bytes.len(), &mut written) })?;
        bytes.truncate(written);
        // The editor only stores valid text; a range that splits a character
        // is clamped to character boundaries by the editor.
        Ok(String::from_utf8(bytes).unwrap_or_else(|error| String::from_utf8_lossy(error.as_bytes()).into_owned()))
    }

    /// Replace a byte range as one undoable edit. With `expected_revision`
    /// the edit fails with `STALE_CHANGE` when the user typed in between.
    pub fn code_editor_replace(&self, name: &str, range: std::ops::Range<usize>, text: &str,
                               expected_revision: Option<u64>) -> VmResult {
        check(unsafe { sys::oneui_view_code_editor_replace(self.raw.as_ptr(),
            sys::OneUiUtf8String::from_str(name), range.start as u64, range.end as u64,
            sys::OneUiUtf8String::from_str(text), expected_revision.unwrap_or(0)) })
    }

    /// Select `anchor..caret` (byte offsets) and scroll it into view.
    pub fn code_editor_select(&self, name: &str, anchor: usize, caret: usize) -> VmResult {
        check(unsafe { sys::oneui_view_code_editor_select(self.raw.as_ptr(),
            sys::OneUiUtf8String::from_str(name), anchor as u64, caret as u64) })
    }
}

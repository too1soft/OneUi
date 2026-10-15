use super::{check, View, VmResult};
use crate::sys;
use std::os::raw::c_int;

/// Toolbar commands of a CodeEditor; keys reach the same actions.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(i32)]
pub enum CodeEditorCommand {
    Undo = 1,
    Redo = 2,
    Find = 3,
    Replace = 4,
    CloseFind = 5,
    SelectAll = 6,
    ToggleComment = 7,
    Indent = 8,
    Outdent = 9,
    DuplicateLines = 10,
    Cut = 11,
    Copy = 12,
    Paste = 13,
}

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

    /// Show document `id` (created empty when unknown; returns true then).
    /// Every document keeps its text, language, undo history, selection and scroll.
    pub fn code_editor_select_document(&self, name: &str, id: &str) -> VmResult<bool> {
        let mut created: c_int = 0;
        check(unsafe { sys::oneui_view_code_editor_select_document(self.raw.as_ptr(),
            sys::OneUiUtf8String::from_str(name), sys::OneUiUtf8String::from_str(id), &mut created) })?;
        Ok(created != 0)
    }

    /// Forget document `id`; closing the shown one shows a new empty document.
    pub fn code_editor_close_document(&self, name: &str, id: &str) -> VmResult {
        check(unsafe { sys::oneui_view_code_editor_close_document(self.raw.as_ptr(),
            sys::OneUiUtf8String::from_str(name), sys::OneUiUtf8String::from_str(id)) })
    }

    /// Replace the shown document's text and clear its undo history.
    pub fn code_editor_reset(&self, name: &str, text: &str) -> VmResult {
        check(unsafe { sys::oneui_view_code_editor_reset(self.raw.as_ptr(),
            sys::OneUiUtf8String::from_str(name), sys::OneUiUtf8String::from_str(text)) })
    }

    /// Run an editing command; false when it did not apply.
    pub fn code_editor_execute(&self, name: &str, command: CodeEditorCommand) -> VmResult<bool> {
        let mut applied: c_int = 0;
        check(unsafe { sys::oneui_view_code_editor_execute(self.raw.as_ptr(),
            sys::OneUiUtf8String::from_str(name), command as c_int, &mut applied) })?;
        Ok(applied != 0)
    }

    /// Whether undo and redo would apply: `(can_undo, can_redo)`.
    pub fn code_editor_history(&self, name: &str) -> VmResult<(bool, bool)> {
        let (mut undo, mut redo): (c_int, c_int) = (0, 0);
        check(unsafe { sys::oneui_view_code_editor_history(self.raw.as_ptr(),
            sys::OneUiUtf8String::from_str(name), &mut undo, &mut redo) })?;
        Ok((undo != 0, redo != 0))
    }
}

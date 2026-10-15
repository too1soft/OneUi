// CodeEditor document access. The editor owns the text; these calls let the
// view model read or edit it on demand. Included after the metrics helpers.
#include "oneui/controls/code_editor.h"
#include "../internal/unicode.h"
namespace {
std::shared_ptr<oneui::CodeEditor> namedCodeEditor(const std::shared_ptr<Runtime>& value, OneUiUtf8String name) {
    auto editor = std::dynamic_pointer_cast<oneui::CodeEditor>(namedViewElement(value, name));
    check(bool(editor), ONEUI_VIEW_TYPE, "Element is not a CodeEditor");
    return editor;
}
}
OneUiViewResult oneui_view_code_editor_state(OneUiView *view, OneUiUtf8String name, OneUiViewCodeEditorState *out) {
    return call(__func__, [&] {
        check(out && out->struct_size == sizeof(*out), ONEUI_VIEW_INVALID_ARGUMENT, "State output size");
        const auto value = runtime(view);
        value->alive();
        const auto state = namedCodeEditor(value, name)->state();
        OneUiViewCodeEditorState result{};
        result.struct_size = sizeof(result);
        result.cursors = static_cast<uint32_t>(state.cursors);
        result.revision = state.revision;
        result.caret = state.caret;
        result.anchor = state.anchor;
        result.line = state.line;
        result.column = state.column;
        result.length = state.length;
        result.line_count = state.lineCount;
        *out = result;
    });
}
OneUiViewResult oneui_view_code_editor_text(OneUiView *view, OneUiUtf8String name, uint64_t start, uint64_t end,
                                           char *buffer, size_t capacity, size_t *out_length) {
    if (out_length) *out_length = 0;
    return call(__func__, [&] {
        check(out_length, ONEUI_VIEW_INVALID_ARGUMENT, "Length output required");
        const auto value = runtime(view);
        value->alive();
        const auto editor = namedCodeEditor(value, name);
        const auto length = editor->byteLength();
        const auto text = start == 0 && end >= length ? editor->textUtf8()
                                                      : editor->textUtf8(static_cast<std::size_t>(std::min<uint64_t>(start, length)),
                                                                         static_cast<std::size_t>(std::min<uint64_t>(end, length)));
        *out_length = text.size();
        if (!buffer) return;
        check(capacity >= text.size(), ONEUI_VIEW_LIMIT, "Text buffer too small");
        std::copy(text.begin(), text.end(), buffer);
    });
}
OneUiViewResult oneui_view_code_editor_replace(OneUiView *view, OneUiUtf8String name, uint64_t start, uint64_t end,
                                              OneUiUtf8String replacement, uint64_t expected_revision) {
    return call(__func__, [&] {
        const auto value = runtime(view);
        value->alive();
        check(callbackPhase == Phase::Idle, ONEUI_VIEW_REENTRANT, "Editor edit during VM callback");
        const auto editor = namedCodeEditor(value, name);
        check(!editor->readOnly(), ONEUI_VIEW_READ_ONLY, "CodeEditor is read-only");
        check(expected_revision == 0 || expected_revision == editor->revision(), ONEUI_VIEW_STALE_CHANGE,
              "CodeEditor changed since the expected revision");
        const auto length = editor->byteLength();
        editor->replaceRange(static_cast<std::size_t>(std::min<uint64_t>(start, length)),
                             static_cast<std::size_t>(std::min<uint64_t>(end, length)),
                             oneui::unicode::fromUtf8(text(replacement)));
    });
}
OneUiViewResult oneui_view_code_editor_select(OneUiView *view, OneUiUtf8String name, uint64_t anchor, uint64_t caret) {
    return call(__func__, [&] {
        const auto value = runtime(view);
        value->alive();
        check(callbackPhase == Phase::Idle, ONEUI_VIEW_REENTRANT, "Editor selection during VM callback");
        const auto editor = namedCodeEditor(value, name);
        const auto length = editor->byteLength();
        editor->setSelection(static_cast<std::size_t>(std::min<uint64_t>(anchor, length)),
                             static_cast<std::size_t>(std::min<uint64_t>(caret, length)));
    });
}
OneUiViewResult oneui_view_code_editor_select_document(OneUiView *view, OneUiUtf8String name, OneUiUtf8String id,
                                                      int *out_created) {
    if (out_created) *out_created = 0;
    return call(__func__, [&] {
        const auto value = runtime(view);
        value->alive();
        check(callbackPhase == Phase::Idle, ONEUI_VIEW_REENTRANT, "Editor document switch during VM callback");
        const bool created = namedCodeEditor(value, name)->selectDocument(oneui::unicode::fromUtf8(text(id)));
        if (out_created) *out_created = created ? 1 : 0;
    });
}
OneUiViewResult oneui_view_code_editor_close_document(OneUiView *view, OneUiUtf8String name, OneUiUtf8String id) {
    return call(__func__, [&] {
        const auto value = runtime(view);
        value->alive();
        check(callbackPhase == Phase::Idle, ONEUI_VIEW_REENTRANT, "Editor document close during VM callback");
        namedCodeEditor(value, name)->closeDocument(oneui::unicode::fromUtf8(text(id)));
    });
}
OneUiViewResult oneui_view_code_editor_reset(OneUiView *view, OneUiUtf8String name, OneUiUtf8String content) {
    return call(__func__, [&] {
        const auto value = runtime(view);
        value->alive();
        check(callbackPhase == Phase::Idle, ONEUI_VIEW_REENTRANT, "Editor reset during VM callback");
        namedCodeEditor(value, name)->setText(oneui::unicode::fromUtf8(text(content)));
    });
}
OneUiViewResult oneui_view_code_editor_execute(OneUiView *view, OneUiUtf8String name, int command, int *out_applied) {
    if (out_applied) *out_applied = 0;
    return call(__func__, [&] {
        check(command >= static_cast<int>(oneui::CodeEditorCommand::Undo) &&
                  command <= static_cast<int>(oneui::CodeEditorCommand::Paste),
              ONEUI_VIEW_INVALID_ARGUMENT, "Unknown CodeEditor command");
        const auto value = runtime(view);
        value->alive();
        check(callbackPhase == Phase::Idle, ONEUI_VIEW_REENTRANT, "Editor command during VM callback");
        const bool applied = namedCodeEditor(value, name)->execute(static_cast<oneui::CodeEditorCommand>(command));
        if (out_applied) *out_applied = applied ? 1 : 0;
    });
}
OneUiViewResult oneui_view_code_editor_history(OneUiView *view, OneUiUtf8String name, int *out_can_undo, int *out_can_redo) {
    return call(__func__, [&] {
        check(out_can_undo && out_can_redo, ONEUI_VIEW_INVALID_ARGUMENT, "History outputs required");
        const auto value = runtime(view);
        value->alive();
        const auto editor = namedCodeEditor(value, name);
        *out_can_undo = editor->canUndo() ? 1 : 0;
        *out_can_redo = editor->canRedo() ? 1 : 0;
    });
}

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

#include "internal/unicode.h"
#include "oneui/controls/text_field.h"
#include "platform/shared/key_mapping.h"
#include "platform/shared/text_input.h"
#include <iostream>

int main() {
    int failures = 0;
    const auto expect = [&](bool passed, const char *name) {
        if (!passed) {
            std::cerr << name << '\n';
            ++failures;
        }
    };
    const std::string utf8 = u8"中文😀A";
    expect(oneui::platform::logicalKey(oneui::platform::asciiVirtualKey('.')) != oneui::Key::Delete,
           "Period must not become Delete in the native key namespace");
    const auto wide = oneui::unicode::fromUtf8(utf8);
    expect(oneui::unicode::toUtf8(wide) == utf8, "UTF-8/native wchar round trip");
    expect(oneui::unicode::toUtf8(oneui::unicode::fromUtf8("\xF0\x28\x8C\x28")) == u8"�(�(",
           "Invalid UTF-8 replacement");
    const auto emoji = oneui::unicode::fromUtf8(u8"😀");
    expect(oneui::unicode::next(emoji, 0) == emoji.size(), "Scalar next");
    expect(oneui::unicode::previous(emoji, emoji.size()) == 0, "Scalar previous");
    const auto document = L"A" + emoji + L"xy" + emoji + L"B";
    const auto selectionStart = 1 + emoji.size();
    const auto deletion = oneui::platform::surroundingDeletionRange(
        document, selectionStart + 2, selectionStart, 1, 1);
    expect(deletion.first == 1 && deletion.second == document.size() - 1,
           "IME byte deletion excludes selection length and rounds outward to scalar boundaries");
    const auto oversized = oneui::platform::surroundingDeletionRange(document, 0, 0, 99999, 99999);
    expect(oversized.first == 0 && oversized.second == document.size(), "IME deletion clamps to document");
    std::wstring longText;
    for (int i = 0; i < 3000; ++i) longText += emoji;
    const auto context = oneui::platform::surroundingText(longText, 1200 * emoji.size(), 1201 * emoji.size());
    expect(context.text.size() <= 4000 && context.caret - context.anchor == 4 &&
           oneui::unicode::toUtf8(oneui::unicode::fromUtf8(context.text)) == context.text,
           "Long IME surrounding context preserves complete UTF-8 scalars and selection offsets");
    oneui::TextField field;
    field.onFocusChanged(true);
    int changed = 0;
    field.setOnChanged([&](const std::wstring &) { ++changed; });
    field.setTextComposition(oneui::unicode::fromUtf8(u8"中文"), 1);
    expect(field.text().empty() && changed == 0, "Preedit does not commit text or notify bindings");
    field.setTextComposition({}, 0);
    expect(field.text().empty() && changed == 0, "Cancel preserves the document");
    field.onTextCommitted(wide);
    expect(field.text() == wide && changed == 1, "IME commits atomically");
    expect(field.undo() && field.text().empty(), "IME commit is one undo operation");
    expect(field.redo() && field.text() == wide, "IME redo");
    field.setText(emoji + L"x");
    field.setCaretIndex(emoji.size());
    oneui::KeyEvent backspace;
    backspace.key = oneui::Key::Backspace;
    field.onKeyDown(backspace);
    expect(field.text() == L"x", "Backspace removes a complete scalar");
    field.setText(emoji + L"x");
    if (sizeof(wchar_t) == 2) {
        field.setCaretIndex(1);
        expect(field.caretIndex() == 0, "Caret cannot split a UTF-16 surrogate pair");
    }
    expect(field.textInputState().identity == &field, "Editor identity follows input focus");
    field.setReadOnly(true);
    expect(!field.onTextCommitted(L"no") && !field.textInputState().editable, "Read-only input is rejected");
    oneui::KeyEvent shortcut;
    shortcut.win = true;
#ifdef __APPLE__
    expect(shortcut.editShortcut(), "Command is the logical edit shortcut");
    shortcut.win = false;
    shortcut.control = true;
    expect(!shortcut.editShortcut(), "Physical Control is not Command");
#else
    expect(!shortcut.editShortcut(), "Meta is not Control");
    shortcut.control = true;
    expect(shortcut.editShortcut(), "Control is the logical edit shortcut");
#endif
    return failures ? 1 : 0;
}

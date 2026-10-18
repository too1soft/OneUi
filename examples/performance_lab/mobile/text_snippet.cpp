#include <oneui/controls/text_field.h>
#include <oneui/layout/stack.h>
#include <oneui/mobile/text_input.h>

// Android: use EditorSurface with its Bridge forwarding snapshots/apply to
// this session. Offsets at that boundary are UTF-16 on every platform.
void exampleMobileTextInput() {
    auto root = std::make_shared<oneui::Stack>();
    auto field = std::make_shared<oneui::TextField>();
    root->add(field);
    oneui::mobile::TextInputSession input(root);
    root->requestFocus(field.get());
    auto state = input.snapshot();
    state.text = u"OneUI 中文";
    state.anchor = state.caret = int(state.text.size());
    input.apply(state);
    // Logical widget coordinates from the same shaped text used for painting.
    const auto handles = input.selectionGeometry();
    if (handles.session) {
        input.selectAtPoint(handles.session, oneui::mobile::SelectionEdge::Insertion,
                            {handles.caretRect.x, handles.caretRect.y});
    }
    // Password snapshots are masked. Forward commands, never reapply bullets.
    field->setPasswordMode(true);
    state = input.snapshot();
    input.editSensitive(state.session, oneui::mobile::SensitiveEdit::Commit, u"test", 1);
    // Call input.revealCaret() after layout when the IME resizes the viewport.
    // Before destroying/backgrounding the native host: input.reset().
}

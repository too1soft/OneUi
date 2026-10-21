#pragma once
#include "oneui/widget.h"
#include <memory>
#include <string>

namespace oneui::mobile {
// UTF-16 offsets at the native IME boundary, independent of sizeof(wchar_t).
struct EditingState {
    uint64_t session = 0;
    std::u16string text;
    int anchor = 0, caret = 0, composingStart = -1, composingEnd = -1;
    bool multiline = false, sensitive = false;
};
// Geometry in the same logical coordinates as Widget::frame(). A zero session
// means unavailable (including sensitive fields and active composition).
struct SelectionGeometry {
    uint64_t session = 0;
    int anchor = 0, caret = 0;
    Rect clip, anchorRect, caretRect;
};
enum class SelectionEdge { Insertion, Anchor, Caret };
enum class SensitiveEdit {
    Commit,
    Compose,
    Select,
    Region,
    Finish,
    DeleteUnits,
    DeleteCodePoints,
    Move
};
// UI-thread-only adapter for TextField / TextArea. Sensitive snapshots contain
// one bullet per UTF-16 unit, never the password. Use editSensitive for those
// editors: applying the masked snapshot as a whole would destroy their text.
// Composition is presentation-only until committed. Custom editors are pending.
// Own alongside WidgetHost; reset before native host destruction/backgrounding.
// An old session cannot write to a new editor or overwrite an external edit.
class TextInputSession final {
  public:
    explicit TextInputSession(std::shared_ptr<Widget> root);
    ~TextInputSession();
    TextInputSession(const TextInputSession &) = delete;
    TextInputSession &operator=(const TextInputSession &) = delete;
    EditingState snapshot();
    bool apply(const EditingState &state);
    // Commit/Compose: first is Android's relative newCursorPosition.
    // Select/Region: first/second are UTF-16 offsets. Delete: before/after counts.
    // Move: first is -1/+1 codepoint, second is whether to extend selection.
    bool editSensitive(uint64_t session, SensitiveEdit command, const std::u16string &text = {},
                       int first = 0, int second = 0);
    bool containsPoint(Point point);
    SelectionGeometry selectionGeometry();
    // Retains the opposite endpoint and session, including crossed selections.
    // Hit testing uses the control's actual shaped layout, never guessed widths.
    bool selectAtPoint(uint64_t session, SelectionEdge edge, Point point);
    void reset();
    // Reapply presentation after the legacy widgets cancel pointer state.
    // Does not commit or change focus; call after a Surface/gesture cancellation.
    void restoreComposition();
    // Called after layout when the keyboard changes the available viewport.
    void revealCaret();

  private:
    struct State;
    std::unique_ptr<State> state_;
    bool applyInternal(const EditingState &state, bool sensitive);
};
} // namespace oneui::mobile

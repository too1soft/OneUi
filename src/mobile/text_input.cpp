#include "oneui/mobile/text_input.h"
#include "oneui/controls/text_field.h"
#include "oneui/layout/scroll_view.h"
#include "text_editing.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <thread>

namespace oneui::mobile {
namespace {
using namespace editing;
std::shared_ptr<TextField> focused(std::shared_ptr<Widget> node) {
    for (unsigned depth = 0; node && depth < 128; ++depth) {
        if (!node->visible() || node->disabled())
            return {};
        if (auto field = std::dynamic_pointer_cast<TextField>(node))
            return field->focused() && field->textInputState().editable ? field : nullptr;
        node = node->activeFocusChild();
    }
    return {};
}
// Private state used for conflict detection and editing only. Public snapshots
// are redacted below; no native password text crosses the JNI read boundary.
TextInputState liveState(const TextField &field) {
    auto live = field.textInputState();
    if (live.sensitive) {
        live.text = field.text();
        live.caret = field.caretIndex();
        live.anchor =
            live.caret == field.selectionStart() ? field.selectionEnd() : field.selectionStart();
    }
    return live;
}
} // namespace
struct TextInputSession::State {
    std::weak_ptr<Widget> root;
    std::weak_ptr<TextField> field;
    std::thread::id thread = std::this_thread::get_id();
    EditingState editing;
    TextInputState base;
    uint64_t serial = 0;
    void check() const {
        if (thread != std::this_thread::get_id())
            throw std::logic_error("TextInputSession requires its UI thread");
    }
};
TextInputSession::TextInputSession(std::shared_ptr<Widget> root) : state_(new State) {
    state_->root = std::move(root);
}
TextInputSession::~TextInputSession() { reset(); }
void TextInputSession::reset() {
    auto &s = *state_;
    s.check();
    if (auto field = s.field.lock())
        field->setTextComposition({}, 0);
    s.field.reset();
    s.editing = {};
    s.base = {};
}
EditingState TextInputSession::snapshot() {
    auto &s = *state_;
    s.check();
    auto field = focused(s.root.lock());
    if (!field) {
        reset();
        return {};
    }
    const auto live = liveState(*field);
    if (field != s.field.lock() || live.session != s.base.session || live.text != s.base.text ||
        live.anchor != s.base.anchor || live.caret != s.base.caret ||
        live.sensitive != s.base.sensitive || field->multiline() != s.editing.multiline) {
        reset();
        s.field = field;
        s.base = live;
        s.editing.session = ++s.serial;
        s.editing.text = encode(live.text);
        s.editing.anchor = int(encode(live.text.substr(0, live.anchor)).size());
        s.editing.caret = int(encode(live.text.substr(0, live.caret)).size());
        s.editing.multiline = field->multiline();
        s.editing.sensitive = live.sensitive;
    }
    auto visible = s.editing;
    if (visible.sensitive)
        visible.text.assign(visible.text.size(), u'\u2022');
    return visible;
}
bool TextInputSession::apply(const EditingState &next) { return applyInternal(next, false); }
bool TextInputSession::applyInternal(const EditingState &next, bool sensitive) {
    auto &s = *state_;
    if (!next.session || snapshot().session != next.session)
        return false;
    auto field = s.field.lock();
    if (s.editing.sensitive != sensitive)
        return false;
    if (!boundary(next.text, next.anchor) || !boundary(next.text, next.caret))
        return false;
    const bool composing = next.composingStart >= 0;
    if (composing
            ? (!boundary(next.text, next.composingStart) ||
               !boundary(next.text, next.composingEnd) || next.composingStart > next.composingEnd)
            : next.composingStart != -1 || next.composingEnd != -1)
        return false;
    // Bound input memory and reject invalid UTF-16 without touching the widget.
    if (next.text.size() > 1024 * 1024)
        return false;
    std::wstring value;
    if (!decode(next.text, value))
        return false;
    if (!field->multiline() && value.find_first_of(L"\r\n") != std::wstring::npos)
        return false;
    if (composing) {
        const size_t start = wideOffset(next.text, next.composingStart);
        const size_t end = wideOffset(next.text, next.composingEnd);
        const auto suffix = value.substr(end);
        const auto &old = field->text();
        size_t originalEnd = old.size() >= suffix.size() ? old.size() - suffix.size() : 0;
        if (originalEnd < start || old.substr(0, start) != value.substr(0, start) ||
            old.substr(originalEnd) != suffix) {
            // An IME may edit outside its composing range in a batch. Commit
            // those changes, keeping the new composing segment as preedit.
            field->replaceTextRange(0, old.size(), value.substr(0, start) + suffix);
            originalEnd = start;
        }
        field->setTextComposition({}, 0);
        field->setSelectionRange(start, originalEnd);
        const size_t caret = wideOffset(next.text, next.caret);
        field->setTextComposition(value.substr(start, end - start),
                                  std::clamp(caret, start, end) - start);
    } else {
        field->setTextComposition({}, 0);
        if (field->text() != value)
            field->replaceTextRange(0, field->text().size(), value);
        field->setSelectionRange(wideOffset(next.text, next.anchor),
                                 wideOffset(next.text, next.caret));
    }
    // Change callbacks can move focus. Never attach an old connection to it.
    if (focused(s.root.lock()) != field) {
        reset();
        return false;
    }
    s.editing = next;
    s.editing.multiline = field->multiline();
    s.base = liveState(*field);
    s.editing.sensitive = s.base.sensitive;
    if (!composing) {
        // Widgets may normalize text/selection in their change callback.
        s.editing.text = encode(s.base.text);
        s.editing.anchor = int(encode(s.base.text.substr(0, s.base.anchor)).size());
        s.editing.caret = int(encode(s.base.text.substr(0, s.base.caret)).size());
    }
    return true;
}
bool TextInputSession::editSensitive(uint64_t session, SensitiveEdit command,
                                     const std::u16string &text, int first, int second) {
    if (!session || snapshot().session != session || !state_->editing.sensitive)
        return false;
    auto next = state_->editing;
    return edit(next, command, text, first, second) && applyInternal(next, true);
}
bool TextInputSession::containsPoint(Point point) {
    state_->check();
    auto field = focused(state_->root.lock());
    if (!field || !field->frame().contains(point))
        return false;
    auto node = state_->root.lock();
    for (unsigned depth = 0; node && node != field && depth < 128; ++depth) {
        if (!node->frame().contains(point))
            return false;
        node = node->activeFocusChild();
    }
    return node == field;
}
SelectionGeometry TextInputSession::selectionGeometry() {
    const auto editing = snapshot();
    if (!editing.session || editing.sensitive || editing.composingStart >= 0)
        return {};
    auto field = state_->field.lock();
    const auto a = field->textRangeRects(wideOffset(editing.text, editing.anchor),
                                         wideOffset(editing.text, editing.anchor));
    const auto c = field->textRangeRects(wideOffset(editing.text, editing.caret),
                                         wideOffset(editing.text, editing.caret));
    if (a.empty() || c.empty())
        return {};
    auto clip = field->frame();
    auto node = state_->root.lock();
    for (unsigned depth = 0; node && node != field && depth < 128; ++depth) {
        const auto frame = node->frame();
        const float left = std::max(clip.x, frame.x), top = std::max(clip.y, frame.y);
        const float right = std::min(clip.x + clip.width, frame.x + frame.width);
        const float bottom = std::min(clip.y + clip.height, frame.y + frame.height);
        clip = {left, top, std::max(0.f, right - left), std::max(0.f, bottom - top)};
        node = node->activeFocusChild();
    }
    if (node != field || clip.width <= 0 || clip.height <= 0)
        return {};
    return {editing.session, editing.anchor, editing.caret, clip, a.front(), c.front()};
}
bool TextInputSession::selectAtPoint(uint64_t session, SelectionEdge edge, Point point) {
    if (!session || !std::isfinite(point.x) || !std::isfinite(point.y))
        return false;
    auto next = snapshot();
    if (next.session != session || next.sensitive || next.composingStart >= 0)
        return false;
    const auto geometry = selectionGeometry();
    if (!geometry.session)
        return false;
    // Keep the hit in the visible editor; a drag cannot select another widget.
    const auto clip = geometry.clip;
    point.x = std::clamp(point.x, clip.x, clip.x + clip.width);
    point.y = std::clamp(point.y, clip.y, clip.y + clip.height);
    auto field = state_->field.lock();
    const auto offset = field->textOffsetAtPoint(point);
    if (!offset)
        return false;
    const int utf16 = int(encode(field->text().substr(0, *offset)).size());
    switch (edge) {
    case SelectionEdge::Insertion:
        next.anchor = next.caret = utf16;
        break;
    case SelectionEdge::Anchor:
        next.anchor = utf16;
        break;
    case SelectionEdge::Caret:
        next.caret = utf16;
        break;
    default:
        return false;
    }
    return applyInternal(next, false);
}
void TextInputSession::revealCaret() {
    auto &s = *state_;
    s.check();
    auto field = focused(s.root.lock());
    if (!field)
        return;
    auto node = s.root.lock();
    for (unsigned depth = 0; node && depth < 128; ++depth) {
        if (auto scroll = std::dynamic_pointer_cast<ScrollView>(node)) {
            const auto caret = field->textInputCaretRect();
            const auto box = scroll->frame();
            const float top = box.y + 8, bottom = box.y + box.height - 8;
            if (bottom > top) {
                const float delta =
                    caret.y < top ? caret.y - top : std::max(0.f, caret.y + caret.height - bottom);
                if (delta != 0)
                    scroll->setScrollOffset(scroll->scrollOffset() + delta);
            }
        }
        node = node->activeFocusChild();
    }
}
void TextInputSession::restoreComposition() {
    if (snapshot().session && state_->editing.composingStart >= 0)
        applyInternal(state_->editing, state_->editing.sensitive);
}
} // namespace oneui::mobile

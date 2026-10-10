#include "oneui/controls/tabs.h"

#include "oneui/icon.h"
#include "oneui/style.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace oneui {
bool Tabs::beginEdit(int index) {
    if (!interactive() || index < 0 || index >= static_cast<int>(items_.size())) {
        return false;
    }
    cancelEdit();
    if (!editor_) {
        editor_ = std::make_shared<TextField>();
        editor_->attachToOwner(this, [this] { invalidate(); },
            [this](Rect rect) { invalidateRect(rect); }, [this] { requestAnimationFrame(); });
        editor_->setOnSubmitted([this](const std::wstring&) { finishEdit(TabEditReason::Submitted); });
    }
    pressedIndex_ = pressedCloseIndex_ = contextPressedIndex_ = -1;
    dragSourceIndex_ = dragTargetIndex_ = -1;
    dragging_ = false;
    editingIndex_ = index;
    ensureIndexVisible(index);
    editor_->setText(items_[static_cast<std::size_t>(index)]);
    const auto style = resolvedItemStyle(index);
    editor_->setFontSize(style.fontSize);
    TextFieldStateStyleOverride normal;
    normal.background = Color{0, 0, 0, 0};
    normal.foreground = style.itemForeground;
    normal.borderWidth = 0.0f;
    normal.padding = Insets{0.0f};
    TextFieldStyleOverride fieldStyle;
    fieldStyle.normal = normal;
    editor_->setStyleOverride(fieldStyle);
    editor_->selectAll();
    editor_->onFocusChanged(true);
    updateEditorFrame();
    invalidate();
    return true;
}

void Tabs::cancelEdit() { finishEdit(TabEditReason::Cancelled); }

int Tabs::editingIndex() const { return editingIndex_; }

Rect Tabs::editorFrame() const { return editingIndex_ >= 0 ? editor_->frame() : Rect{}; }

void Tabs::setOnEditFinished(std::function<void(int, const std::wstring&, TabEditReason)> callback) {
    onEditFinished_ = std::move(callback);
}

void Tabs::finishEdit(TabEditReason reason) {
    if (editingIndex_ < 0) { return; }
    const int index = editingIndex_;
    const std::wstring value = editor_->text();
    editingIndex_ = -1;
    editor_->onFocusChanged(false);
    invalidate();
    // Clear the edit state before notifying; consumers may replace the items.
    const auto callback = onEditFinished_;
    if (callback) { callback(index, value, reason); }
}

void Tabs::updateEditorFrame() {
    if (editingIndex_ < 0) { return; }
    Rect rect = textRect(editingIndex_);
    const float right = std::min(rect.x + rect.width, frame().x + frame().width);
    rect.x = std::max(rect.x, frame().x);
    rect.width = std::max(0.0f, right - rect.x);
    editor_->setFrame(rect);
    editor_->setTextEnvironment(textFontFamily(), textDpiScale());
}

bool Tabs::onFocusChanged(bool focused) {
    const bool changed = Widget::onFocusChanged(focused);
    if (editingIndex_ >= 0 && !focused) { finishEdit(TabEditReason::FocusLost); }
    return changed;
}

bool Tabs::onTextInput(wchar_t character) {
    return editingIndex_ >= 0 && editor_->onTextInput(character);
}

bool Tabs::onTextCommitted(const std::wstring& text) {
    return editingIndex_ >= 0 && editor_->onTextCommitted(text);
}

TextInputState Tabs::textInputState() const {
    return editingIndex_ >= 0 ? editor_->textInputState() : TextInputState{};
}

bool Tabs::hasTextComposition() const { return editingIndex_ >= 0 && editor_->hasTextComposition(); }

void Tabs::setTextComposition(std::wstring text, std::size_t caret) {
    if (editingIndex_ >= 0) { editor_->setTextComposition(std::move(text), caret); }
}

bool Tabs::replaceTextRange(std::size_t start, std::size_t end, const std::wstring& text) {
    return editingIndex_ >= 0 && editor_->replaceTextRange(start, end, text);
}

Rect Tabs::textInputCaretRect() const {
    return editingIndex_ >= 0 ? editor_->textInputCaretRect() : Widget::textInputCaretRect();
}

std::shared_ptr<Widget> Tabs::activeFocusChild() const {
    return editingIndex_ >= 0 ? editor_ : nullptr;
}

bool Tabs::tickAnimations(double nowMs) {
    return editingIndex_ >= 0 && editor_->tickAnimations(nowMs);
}

CursorKind Tabs::cursor(Point point) const {
    return editingIndex_ >= 0 && editor_->frame().contains(point)
        ? CursorKind::Text : Widget::cursor(point);
}

bool Tabs::onKeyDown(const KeyEvent& event) {
    if (!interactive() || items_.empty()) {
        return false;
    }
    if (editingIndex_ >= 0) {
        if (event.key == Key::Escape) {
            if (hasTextComposition()) { editor_->setTextComposition({}, 0); }
            else { cancelEdit(); }
            return true;
        }
        if (event.key == Key::Tab) { finishEdit(TabEditReason::FocusLost); return false; }
        return editor_->onKeyDown(event);
    }

    if (event.key == Key::Left || event.key == Key::Up) {
        assignSelectedIndex(selectedIndex() - 1);
        return true;
    }

    if (event.key == Key::Right || event.key == Key::Down) {
        assignSelectedIndex(selectedIndex() + 1);
        return true;
    }

    if (event.key == Key::Home) {
        assignSelectedIndex(0);
        return true;
    }

    if (event.key == Key::End) {
        assignSelectedIndex(static_cast<int>(items_.size()) - 1);
        return true;
    }

    return false;
}

bool Tabs::isFocusable() const {
    return interactive() && !items_.empty();
}

AccessibilityInfo Tabs::accessibilityInfo() const {
    auto info = Widget::accessibilityInfo();
    if (info.role == AccessibilityRole::None) {
        info.role = AccessibilityRole::TabList;
    }
    if (!items_.empty()) {
        const int index = std::clamp(selectedIndex(), 0, static_cast<int>(items_.size()) - 1);
        info.value = items_[static_cast<std::size_t>(index)];
        info.state.selected = true;
    }
    return info;
}

} // namespace oneui

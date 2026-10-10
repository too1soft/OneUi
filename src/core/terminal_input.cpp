#include "terminal_view_helpers.h"

namespace oneui {
using namespace terminal_detail;

void TerminalView::setOnTextInput(TextInputCallback callback) {
    onTextInput_ = std::move(callback);
}

void TerminalView::setOnPaste(PasteCallback callback) {
    onPaste_ = std::move(callback);
}

void TerminalView::setOnRawKey(RawKeyCallback callback) {
    onRawKey_ = std::move(callback);
}

void TerminalView::setOnScroll(ScrollCallback callback) {
    onScroll_ = std::move(callback);
    wheelRowRemainder_ = 0.0f;
}

void TerminalView::setOnPointer(PointerCallback callback) {
    onPointer_ = std::move(callback);
    reportedButton_ = MouseButton::None;
    pointerWheelRemainder_ = 0.0f;
}

void TerminalView::setRightButtonAction(TerminalAuxiliaryButtonAction action) {
    rightButtonAction_ = action;
}

void TerminalView::setMiddleButtonAction(TerminalAuxiliaryButtonAction action) {
    middleButtonAction_ = action;
}

void TerminalView::setOnHyperlink(HyperlinkCallback callback) {
    onHyperlink_ = std::move(callback);
    pressedHyperlink_ = 0;
}

void TerminalView::setOnViewportChanged(ViewportChangedCallback callback) {
    onViewportChanged_ = std::move(callback);
    viewport_ = {};
    invalidate();
}

void TerminalView::setOnFocusChanged(FocusChangedCallback callback) {
    onFocusChanged_ = std::move(callback);
}

void TerminalView::setAnimationScheduler(std::function<void()> scheduler) {
    Widget::setAnimationScheduler(std::move(scheduler));
    if ((focused() && cursorBlinking_ && cursor_.visible) || hasBlinkingText()) {
        requestAnimationFrame();
    }
}

bool TerminalView::onKeyDown(const KeyEvent& event) {
    if (!interactive()) {
        return false;
    }
    if (isCopyShortcut(event)) {
        copySelectionToClipboard();
        return true;
    }
    if (isPasteShortcut(event)) {
        pasteFromClipboard();
        return true;
    }
    if (!isModifierKey(event) && hasSelection()) {
        clearSelection();
    }
    if (onRawKey_) {
        onRawKey_(event);
    }
    restartCursorBlink();
    return true;
}

bool TerminalView::onKeyUp(const KeyEvent& event) {
    if (!interactive()) {
        return false;
    }
    if (isCopyShortcut(event) || isPasteShortcut(event)) {
        return true;
    }
    KeyEvent released = event;
    released.pressed = false;
    released.repeat = false;
    if (onRawKey_) {
        onRawKey_(released);
    }
    return true;
}

bool TerminalView::onTextInput(wchar_t character) {
    if (!interactive() || character == L'\0') {
        return false;
    }
    if (hasSelection()) {
        clearSelection();
    }
    if (onTextInput_) {
        onTextInput_(std::wstring(1, character));
    }
    restartCursorBlink();
    return true;
}

bool TerminalView::onTextInputText(const std::wstring& text) {
    if (!interactive() || text.empty()) {
        return false;
    }
    if (hasSelection()) {
        clearSelection();
    }
    if (onTextInput_) {
        onTextInput_(text);
    }
    restartCursorBlink();
    return true;
}

Rect TerminalView::textInputCaretRect() const {
    const Rect bounds = frame();
    const float cellWidth = std::max(1.0f, lastMetrics_.cellWidth);
    const float cellHeight = std::max(1.0f, lastMetrics_.cellHeight);
    const float x = contentBounds(bounds, lastMetrics_).x +
                    cellWidth * static_cast<float>(cursor_.column);
    const float y = bounds.y + cellHeight * static_cast<float>(cursor_.row);
    return Rect{x, y, cellWidth, cellHeight};
}

bool TerminalView::onFocusChanged(bool focused) {
    if (!Widget::onFocusChanged(focused)) {
        return false;
    }
    if (focused) {
        restartCursorBlink();
    } else if (!cursorBlinkVisible_) {
        cursorBlinkVisible_ = true;
        invalidateCell(TextPosition{cursor_.row, cursor_.column});
    }
    if (onFocusChanged_) {
        onFocusChanged_(focused);
    }
    return true;
}

bool TerminalView::isFocusable() const {
    return interactive();
}

CursorKind TerminalView::cursor(Point point) const {
    if (!contains(point)) {
        return CursorKind::Default;
    }
    if (hyperlinkAt(point) != 0) {
        return CursorKind::Pointer;
    }
    return !mouseReporting_ ? CursorKind::Text : CursorKind::Default;
}

AccessibilityInfo TerminalView::accessibilityInfo() const {
    auto info = Widget::accessibilityInfo();
    if (info.role == AccessibilityRole::None) {
        info.role = AccessibilityRole::Custom;
    }
    if (info.name.empty()) {
        info.name = L"Terminal";
    }
    info.state.readOnly = !onTextInput_ && !onRawKey_;
    return info;
}

} // namespace oneui

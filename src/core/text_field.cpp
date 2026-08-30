#include "oneui/controls/text_field.h"

#include "oneui/icon.h"
#include "oneui/style.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>

namespace oneui {
namespace {

constexpr float ApproxCharacterWidth = 7.0f;
constexpr float AffixIconSize = 14.0f;
constexpr float AffixIconGap = 8.0f;
constexpr float CaretWidth = 1.0f;
constexpr float CaretMinHeight = 11.0f;
constexpr float CaretMaxHeight = 14.0f;
constexpr float CaretVisualInset = 1.0f;
constexpr double CaretBlinkPeriodMs = 1060.0;
constexpr double CaretBlinkOnMs = 530.0;

float approximateGlyphWidth(wchar_t character) {
    switch (character) {
    case L'W':
    case L'M':
    case L'@':
    case L'#':
        return 10.0f;
    case L'i':
    case L'l':
    case L'I':
    case L'!':
    case L'|':
    case L' ':
        return 4.0f;
    default:
        return ApproxCharacterWidth;
    }
}

void applyFocusRingOverride(FocusRingStyle& style, const FocusRingStyleOverride& override) {
    if (override.color) {
        style.color = *override.color;
    }
    if (override.width) {
        style.width = *override.width;
    }
    if (override.offset) {
        style.offset = *override.offset;
    }
    if (override.radius) {
        style.radius = *override.radius;
    }
    if (override.visible) {
        style.visible = *override.visible;
    }
}

void applyTextFieldStateOverride(TextFieldStyle& style, const TextFieldStateStyleOverride& override) {
    if (override.background) {
        style.background = *override.background;
    }
    if (override.foreground) {
        style.foreground = *override.foreground;
    }
    if (override.placeholderForeground) {
        style.placeholderForeground = *override.placeholderForeground;
    }
    if (override.border) {
        style.border = *override.border;
    }
    if (override.selectionBackground) {
        style.selectionBackground = *override.selectionBackground;
    }
    if (override.caretColor) {
        style.caretColor = *override.caretColor;
    }
    if (override.borderWidth) {
        style.borderWidth = *override.borderWidth;
    }
    if (override.radius) {
        style.radius = *override.radius;
    }
    if (override.padding) {
        style.padding = *override.padding;
    }
    if (override.focusRing) {
        applyFocusRingOverride(style.focusRing, *override.focusRing);
    }
    if (override.transition) {
        style.transition = *override.transition;
    }
    if (override.shadows) {
        style.shadows = *override.shadows;
    }
}

TextFieldStyle baseTextFieldStyle(bool disabled, bool readOnly, bool hovered) {
    const auto& t = theme();
    TextFieldStyle style;
    style.background = (disabled || readOnly) ? t.surfaceMuted : t.surface;
    style.foreground = disabled ? t.textSubtle : t.text;
    style.placeholderForeground = t.textSubtle;
    style.border = disabled ? t.border : (hovered ? t.borderStrong : t.border);
    style.selectionBackground = disabled ? Color{203, 213, 225} : Color{191, 219, 254};
    style.caretColor = t.primary;
    style.borderWidth = 1.0f;
    style.radius = t.radiusMd;
    style.padding = Insets{0.0f, 12.0f};
    style.focusRing = FocusRingStyle{t.focusOutline, t.focusOutlineWidth, t.focusOutlineOffset, t.radiusLg, true};
    return style;
}

void paintAffixIcon(Canvas& canvas, IconSymbol symbol, Rect rect, Color color) {
    paintIcon(canvas, symbol, rect, color, Color{0, 0, 0, 0}, 1.4f);
}

double currentTimeMs() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration<double, std::milli>(now).count();
}

} // namespace

TextField::TextField(std::wstring placeholder) : placeholder_(std::move(placeholder)) {
    setPreferredSize(Size{194.0f, 36.0f});
}

TextArea::TextArea(std::wstring placeholder) : TextField(std::move(placeholder)) {
    setMultiline(true);
}

void TextField::setPlaceholder(std::wstring placeholder) {
    placeholder_ = std::move(placeholder);
    invalidate();
}

void TextField::setText(std::wstring text) {
    const std::size_t nextCaretIndex = text.size();
    if (!assignText(std::move(text), nextCaretIndex)) {
        clampCaret();
    }
    clearEditHistory();
}

const std::wstring& TextField::text() const {
    return value();
}

void TextField::setCaretIndex(std::size_t index) {
    setCaretIndexInternal(index);
}

std::size_t TextField::caretIndex() const {
    return std::min(caretIndex_, value().size());
}

void TextField::setSelectionRange(std::size_t start, std::size_t end) {
    const std::size_t size = value().size();
    selectionAnchor_ = std::min(start, size);
    caretIndex_ = std::min(end, size);
    hasSelection_ = selectionAnchor_ != caretIndex_;
    ensureCaretVisible();
    invalidate();
}

std::size_t TextField::selectionStart() const {
    if (!hasSelection()) {
        return caretIndex();
    }
    return std::min(selectionAnchor_, caretIndex());
}

std::size_t TextField::selectionEnd() const {
    if (!hasSelection()) {
        return caretIndex();
    }
    return std::max(selectionAnchor_, caretIndex());
}

bool TextField::hasSelection() const {
    return hasSelection_ && selectionAnchor_ != caretIndex_;
}

std::wstring TextField::selectedText() const {
    if (!hasSelection()) {
        return {};
    }
    return value().substr(selectionStart(), selectionEnd() - selectionStart());
}

void TextField::selectAll() {
    setSelectionRange(0, value().size());
}

void TextField::clearSelection() {
    if (!hasSelection_) {
        return;
    }
    hasSelection_ = false;
    selectionAnchor_ = caretIndex();
    invalidate();
}

bool TextField::copySelectionToClipboard(Clipboard& clipboard) const {
    if (!hasSelection()) {
        return false;
    }
    clipboard.setText(selectedText());
    return true;
}

bool TextField::cutSelectionToClipboard(Clipboard& clipboard) {
    if (!editable()) {
        return false;
    }
    if (!copySelectionToClipboard(clipboard)) {
        return false;
    }
    return deleteSelection();
}

bool TextField::pasteFromClipboard(const Clipboard& clipboard) {
    if (!editable()) {
        return false;
    }

    const std::wstring pastedText = clipboard.text();
    if (pastedText.empty()) {
        return false;
    }

    std::wstring next = value();
    const std::size_t insertionIndex = hasSelection() ? selectionStart() : caretIndex();
    if (hasSelection()) {
        next.erase(selectionStart(), selectionEnd() - selectionStart());
    }
    next.insert(insertionIndex, pastedText);
    return assignText(std::move(next), insertionIndex + pastedText.size(), true);
}

bool TextField::undo() {
    if (!editable() || undoStack_.empty()) {
        return false;
    }

    const TextEditEntry entry = std::move(undoStack_.back());
    undoStack_.pop_back();
    redoStack_.push_back(entry);
    if (!restoreEditSnapshot(entry.before)) {
        return false;
    }
    return true;
}

bool TextField::redo() {
    if (!editable() || redoStack_.empty()) {
        return false;
    }

    const TextEditEntry entry = std::move(redoStack_.back());
    redoStack_.pop_back();
    undoStack_.push_back(entry);
    if (!restoreEditSnapshot(entry.after)) {
        return false;
    }
    return true;
}

void TextField::setReadOnly(bool readOnly) {
    if (readOnly_ == readOnly) {
        return;
    }
    const TextFieldStyle previous = resolvedStyle();
    readOnly_ = readOnly;
    beginVisualTransition(previous, resolvedStyle());
    invalidate();
}

bool TextField::readOnly() const {
    return readOnly_;
}

void TextField::setMultiline(bool multiline) {
    if (multiline_ == multiline) {
        return;
    }
    multiline_ = multiline;
    textScrollOffset_ = 0;
    verticalScrollLine_ = 0;
    horizontalScrollOffset_ = 0.0f;
    invalidateTextMetrics();
    setPreferredSize(multiline ? Size{320.0f, 160.0f} : Size{194.0f, 36.0f});
    invalidate();
}

bool TextField::multiline() const {
    return multiline_;
}

void TextField::setLineHeight(float lineHeight) {
    const float next = std::max(12.0f, lineHeight);
    if (std::fabs(lineHeight_ - next) < 0.01f) {
        return;
    }
    lineHeight_ = next;
    ensureCaretVisible();
    invalidate();
}

float TextField::lineHeight() const {
    return lineHeight_;
}

void TextField::setClipboard(std::shared_ptr<Clipboard> clipboard) {
    clipboard_ = std::move(clipboard);
}

std::shared_ptr<Clipboard> TextField::clipboard() const {
    return clipboard_;
}

void TextField::setPasswordMode(bool enabled) {
    if (passwordMode_ == enabled) {
        return;
    }
    passwordMode_ = enabled;
    invalidateTextMetrics();
    invalidate();
}

bool TextField::passwordMode() const {
    return passwordMode_;
}

void TextField::setPasswordMask(wchar_t mask) {
    if (passwordMask_ == mask) {
        return;
    }
    passwordMask_ = mask;
    invalidateTextMetrics();
    invalidate();
}

wchar_t TextField::passwordMask() const {
    return passwordMask_;
}

void TextField::setPrefixIcon(std::optional<IconSymbol> symbol) {
    prefixIcon_ = symbol;
    invalidate();
}

void TextField::setSuffixIcon(std::optional<IconSymbol> symbol) {
    suffixIcon_ = symbol;
    invalidate();
}

void TextField::setStyleOverride(TextFieldStyleOverride style) {
    const TextFieldStyle previous = resolvedStyle();
    styleOverride_ = std::move(style);
    beginVisualTransition(previous, resolvedStyle());
    invalidate();
}

void TextField::clearStyleOverride() {
    const TextFieldStyle previous = resolvedStyle();
    styleOverride_.reset();
    beginVisualTransition(previous, resolvedStyle());
    invalidate();
}

void TextField::bindText(State<std::wstring>& state) {
    textBinding_ = Binding<std::wstring>(state, [this] {
        text_ = value();
        invalidateTextMetrics();
        clampCaret();
        clampSelection();
        if (!applyingInternalTextChange_) {
            clearEditHistory();
        }
        invalidate();
    });
    text_ = value();
    setCaretIndexInternal(text_.size());
    clearEditHistory();
    invalidate();
}

void TextField::setOnChanged(std::function<void(const std::wstring&)> callback) {
    onChanged_ = std::move(callback);
}

void TextField::setOnSubmitted(std::function<void(const std::wstring&)> callback) {
    onSubmitted_ = std::move(callback);
}

void TextField::setDisabled(bool disabled) {
    const TextFieldStyle previous = resolvedStyle();
    Widget::setDisabled(disabled);
    beginVisualTransition(previous, resolvedStyle());
}

void TextField::setAnimationScheduler(std::function<void()> scheduler) {
    Widget::setAnimationScheduler(std::move(scheduler));
    if (focused() && editable()) {
        requestAnimationFrame();
    }
}

void TextField::paint(Canvas& canvas) {
    const Rect rect = frame();
    const bool hasText = !value().empty();
    const bool shouldPaintPlaceholder = !hasText && !(focused() && editable());
    const TextFieldStyle style = visualStyle(resolvedStyle());

    if (focusVisible() && !disabled() && style.focusRing.visible) {
        const float offset = style.focusRing.offset;
        canvas.strokeRect(Rect{rect.x - offset, rect.y - offset, rect.width + offset * 2.0f, rect.height + offset * 2.0f}, style.focusRing.color, style.focusRing.radius, style.focusRing.width);
    }

    for (const auto& shadow : style.shadows) {
        if (!shadow.inset) {
            canvas.drawBoxShadow(
                Rect{rect.x + shadow.offset.x, rect.y + shadow.offset.y, rect.width, rect.height},
                BoxShadow{shadow.color, Point{0.0f, 0.0f}, shadow.blurRadius, shadow.spreadRadius},
                style.radius);
        }
    }

    canvas.fillRect(rect, style.background, style.radius);
    canvas.strokeRect(rect, style.border, style.radius, style.borderWidth);
    for (const auto& shadow : style.shadows) {
        if (shadow.inset) {
            canvas.strokeRect(rect, shadow.color, style.radius, std::max(1.0f, shadow.blurRadius));
        }
    }

    Rect insetRect = rect.inset(style.padding);
    const float affixSpace = AffixIconSize + AffixIconGap;
    if (prefixIcon_) {
        const Rect iconRect{insetRect.x, rect.y + (rect.height - AffixIconSize) / 2.0f, AffixIconSize, AffixIconSize};
        paintAffixIcon(canvas, *prefixIcon_, iconRect, hasText ? style.foreground : style.placeholderForeground);
        insetRect.x += affixSpace;
        insetRect.width = std::max(0.0f, insetRect.width - affixSpace);
    }
    if (suffixIcon_) {
        const Rect iconRect{insetRect.x + std::max(0.0f, insetRect.width - AffixIconSize), rect.y + (rect.height - AffixIconSize) / 2.0f, AffixIconSize, AffixIconSize};
        paintAffixIcon(canvas, *suffixIcon_, iconRect, style.placeholderForeground);
        insetRect.width = std::max(0.0f, insetRect.width - affixSpace);
    }
    const Rect contentRect{insetRect.x, insetRect.y, std::max(0.0f, insetRect.width), std::max(0.0f, insetRect.height)};
    if (multiline_) {
        paintMultilineContent(canvas, contentRect, style, hasText, shouldPaintPlaceholder);
        return;
    }
    updateTextMetrics(&canvas);
    ensureCaretVisible();
    const float scrollX = textWidthAt(textScrollOffset_);

    canvas.save();
    canvas.clipRect(contentRect);

    if (hasSelection() && hasText) {
        const float selectionX = contentRect.x + textWidthAt(selectionStart()) - scrollX;
        const float selectionWidth = textWidthAt(selectionEnd()) - textWidthAt(selectionStart());
        canvas.fillRect(Rect{selectionX, contentRect.y + 8.0f, selectionWidth, std::max(0.0f, contentRect.height - 16.0f)}, style.selectionBackground, 3.0f);
    }

    if (hasText || shouldPaintPlaceholder) {
        canvas.drawText(hasText ? measuredDisplayText_ : placeholder_, Rect{contentRect.x - scrollX, contentRect.y, contentRect.width + scrollX, contentRect.height}, hasText ? style.foreground : style.placeholderForeground, theme().fontMd, TextAlign::Left);
    }

    if (focused() && editable() && caretBlinkVisible_) {
        float caretX = contentRect.x + textWidthAt(caretIndex()) - scrollX + CaretVisualInset;
        if (caretIndex() > textScrollOffset_) {
            caretX += 1.0f;
        }
        caretX = std::floor(caretX) + 0.5f;
        const float caretHeight = std::min(CaretMaxHeight, std::max(CaretMinHeight, contentRect.height - 22.0f));
        const float caretY = contentRect.y + (contentRect.height - caretHeight) * 0.5f;
        canvas.fillRect(Rect{caretX, caretY, CaretWidth, caretHeight}, style.caretColor, 0.5f);
    }

    canvas.restore();
}

bool TextField::onMouseMove(const MouseEvent& event) {
    if (!interactive()) {
        return false;
    }

    if (selecting_) {
        setCaretIndexInternal(caretIndexFromPoint(event.position), true);
        return true;
    }

    const bool next = contains(event.position);
    if (next == hovered_) {
        return false;
    }
    const TextFieldStyle previous = resolvedStyle();
    hovered_ = next;
    beginVisualTransition(previous, resolvedStyle());
    invalidate();
    return true;
}

bool TextField::onMouseDown(const MouseEvent& event) {
    if (!interactive() || !contains(event.position)) {
        return false;
    }
    selecting_ = true;
    setCaretIndexInternal(caretIndexFromPoint(event.position));
    restartCaretBlink();
    return true;
}

bool TextField::onMouseUp(const MouseEvent&) {
    if (!selecting_) {
        return false;
    }
    selecting_ = false;
    invalidate();
    return true;
}

bool TextField::onKeyDown(const KeyEvent& event) {
    if (!interactive()) {
        return false;
    }

    if (event.control) {
        if (event.key == Key::A) {
            selectAll();
            return true;
        }
        if (event.key == Key::C && clipboard_) {
            return copySelectionToClipboard(*clipboard_);
        }
        if (event.key == Key::X && clipboard_ && editable()) {
            return cutSelectionToClipboard(*clipboard_);
        }
        if (event.key == Key::V && clipboard_ && editable()) {
            return pasteFromClipboard(*clipboard_);
        }
        return false;
    }

    if (event.key == Key::Left) {
        if (!event.shift && hasSelection()) {
            setCaretIndexInternal(selectionStart());
            return true;
        }
        if (caretIndex() == 0) {
            return false;
        }
        setCaretIndexInternal(caretIndex() - 1, event.shift);
        return true;
    }

    if (event.key == Key::Right) {
        if (!event.shift && hasSelection()) {
            setCaretIndexInternal(selectionEnd());
            return true;
        }
        if (caretIndex() >= value().size()) {
            return false;
        }
        setCaretIndexInternal(caretIndex() + 1, event.shift);
        return true;
    }

    if (multiline_ && event.key == Key::Up) {
        return moveCaretVertically(-1, event.shift);
    }

    if (multiline_ && event.key == Key::Down) {
        return moveCaretVertically(1, event.shift);
    }

    if (event.key == Key::Home) {
        std::size_t target = 0;
        if (multiline_) {
            updateMultilineTextMetrics();
            const std::size_t line = multilineLineIndexForCaret(caretIndex());
            target = measuredLines_[line].start;
        }
        if (caretIndex() == target) {
            return false;
        }
        setCaretIndexInternal(target, event.shift);
        return true;
    }

    if (event.key == Key::End) {
        std::size_t target = value().size();
        if (multiline_) {
            updateMultilineTextMetrics();
            const std::size_t line = multilineLineIndexForCaret(caretIndex());
            target = measuredLines_[line].end;
        }
        if (caretIndex() == target) {
            return false;
        }
        setCaretIndexInternal(target, event.shift);
        return true;
    }

    if (event.key == Key::Enter) {
        if (!multiline_) {
            if (!onSubmitted_) {
                return false;
            }
            onSubmitted_(value());
            return true;
        }
        if (!editable()) {
            return false;
        }
        std::wstring next = value();
        const std::size_t insertionIndex = hasSelection() ? selectionStart() : caretIndex();
        if (hasSelection()) {
            next.erase(selectionStart(), selectionEnd() - selectionStart());
        }
        next.insert(next.begin() + static_cast<std::wstring::difference_type>(insertionIndex), L'\n');
        assignText(std::move(next), insertionIndex + 1, true);
        return true;
    }

    if (event.key == Key::Delete) {
        if (!editable()) {
            return false;
        }
        if (deleteSelection()) {
            return true;
        }
        if (caretIndex() >= value().size()) {
            return false;
        }
        std::wstring next = value();
        const std::size_t removalIndex = caretIndex();
        next.erase(removalIndex, 1);
        assignText(std::move(next), removalIndex, true);
        return true;
    }

    if (event.key != Key::Backspace || value().empty() || !editable()) {
        return false;
    }

    if (deleteSelection()) {
        return true;
    }

    if (caretIndex() == 0) {
        return false;
    }

    std::wstring next = value();
    const std::size_t removalIndex = caretIndex() - 1;
    next.erase(removalIndex, 1);
    assignText(std::move(next), removalIndex, true);
    return true;
}

bool TextField::onTextInput(wchar_t character) {
    if (!editable()) {
        return false;
    }
    if (character == L'\r' || character == L'\n') {
        if (!multiline_) {
            return false;
        }
        character = L'\n';
    }
    std::wstring next = value();
    const std::size_t insertionIndex = hasSelection() ? selectionStart() : caretIndex();
    if (hasSelection()) {
        next.erase(selectionStart(), selectionEnd() - selectionStart());
    }
    next.insert(next.begin() + static_cast<std::wstring::difference_type>(insertionIndex), character);
    assignText(std::move(next), insertionIndex + 1, true);
    restartCaretBlink();
    return true;
}

bool TextField::onFocusChanged(bool focused) {
    const TextFieldStyle previous = resolvedStyle();
    if (!Widget::onFocusChanged(focused)) {
        return false;
    }
    beginVisualTransition(previous, resolvedStyle());
    if (focused && editable()) {
        restartCaretBlink();
    } else {
        caretBlinkVisible_ = true;
        invalidate();
    }
    return true;
}

CursorKind TextField::cursor(Point point) const {
    return interactive() && !readOnly() && contains(point) ? CursorKind::Text : CursorKind::Default;
}

bool TextField::isFocusable() const {
    return interactive();
}

void TextField::setFocusVisible(bool visible) {
    const TextFieldStyle previous = resolvedStyle();
    const bool oldFocusVisible = focusVisible();
    Widget::setFocusVisible(visible);
    if (oldFocusVisible == focusVisible()) {
        return;
    }
    beginVisualTransition(previous, resolvedStyle());
}

bool TextField::tickAnimations(double nowMs) {
    bool keepScheduling = false;
    bool needsPaint = false;
    needsPaint = backgroundTransition_.tick(nowMs) || needsPaint;
    needsPaint = foregroundTransition_.tick(nowMs) || needsPaint;
    needsPaint = placeholderTransition_.tick(nowMs) || needsPaint;
    needsPaint = borderTransition_.tick(nowMs) || needsPaint;
    keepScheduling = backgroundTransition_.running() ||
                     foregroundTransition_.running() ||
                     placeholderTransition_.running() ||
                     borderTransition_.running();
    if (focused() && editable()) {
        const double elapsed = std::fmod(std::max(0.0, nowMs - caretBlinkStartMs_), CaretBlinkPeriodMs);
        const bool nextCaretVisible = elapsed < CaretBlinkOnMs;
        if (nextCaretVisible != caretBlinkVisible_) {
            caretBlinkVisible_ = nextCaretVisible;
            needsPaint = true;
        }
        keepScheduling = true;
    } else if (!caretBlinkVisible_) {
        caretBlinkVisible_ = true;
        needsPaint = true;
    }
    if (needsPaint) {
        invalidate();
    }
    return keepScheduling;
}

AccessibilityInfo TextField::accessibilityInfo() const {
    auto info = Widget::accessibilityInfo();
    if (info.role == AccessibilityRole::None) {
        info.role = AccessibilityRole::TextBox;
    }
    if (info.name.empty()) {
        info.name = placeholder_;
    }
    info.value = passwordMode_ ? displayText() : value();
    info.state.readOnly = readOnly_;
    return info;
}

bool TextField::assignText(std::wstring text, std::size_t nextCaretIndex, bool recordUndo) {
    const std::wstring previous = value();
    if (text == previous) {
        return false;
    }

    const TextEditSnapshot before = makeEditSnapshot();

    applyingInternalTextChange_ = true;
    textBinding_.set(std::move(text), text_);
    applyingInternalTextChange_ = false;
    text_ = value();
    invalidateTextMetrics();
    setCaretIndexInternal(nextCaretIndex);
    clearSelection();
    const std::wstring current = text_;
    if (current == previous) {
        return false;
    }

    if (recordUndo) {
        undoStack_.push_back(TextEditEntry{before, makeEditSnapshot()});
        redoStack_.clear();
    }

    invalidate();
    if (onChanged_) {
        onChanged_(current);
    }
    restartCaretBlink();
    return true;
}

bool TextField::editable() const {
    return interactive() && !readOnly_;
}

TextField::TextEditSnapshot TextField::makeEditSnapshot() const {
    return TextEditSnapshot{value(), caretIndex(), selectionAnchor_, hasSelection()};
}

bool TextField::restoreEditSnapshot(const TextEditSnapshot& snapshot) {
    const std::wstring previous = value();
    if (snapshot.text == previous) {
        caretIndex_ = std::min(snapshot.caretIndex, value().size());
        selectionAnchor_ = std::min(snapshot.selectionAnchor, value().size());
        hasSelection_ = snapshot.hasSelection && selectionAnchor_ != caretIndex_;
        ensureCaretVisible();
        invalidate();
        return false;
    }

    applyingInternalTextChange_ = true;
    textBinding_.set(snapshot.text, text_);
    applyingInternalTextChange_ = false;
    text_ = value();
    invalidateTextMetrics();
    caretIndex_ = std::min(snapshot.caretIndex, text_.size());
    selectionAnchor_ = std::min(snapshot.selectionAnchor, text_.size());
    hasSelection_ = snapshot.hasSelection && selectionAnchor_ != caretIndex_;
    ensureCaretVisible();
    invalidate();

    const std::wstring current = text_;
    if (onChanged_) {
        onChanged_(current);
    }
    return true;
}

void TextField::clearEditHistory() {
    undoStack_.clear();
    redoStack_.clear();
}

void TextField::clampCaret() {
    const std::size_t next = std::min(caretIndex_, value().size());
    if (next != caretIndex_) {
        caretIndex_ = next;
        invalidate();
    }
    ensureCaretVisible();
}

void TextField::clampSelection() {
    if (!hasSelection_) {
        selectionAnchor_ = caretIndex();
        return;
    }
    const std::size_t size = value().size();
    selectionAnchor_ = std::min(selectionAnchor_, size);
    caretIndex_ = std::min(caretIndex_, size);
    hasSelection_ = selectionAnchor_ != caretIndex_;
    ensureCaretVisible();
}

void TextField::setCaretIndexInternal(std::size_t index, bool extendSelection) {
    const std::size_t next = std::min(index, value().size());
    if (next == caretIndex_ && (extendSelection ? hasSelection_ : !hasSelection_)) {
        return;
    }
    if (extendSelection && !hasSelection_) {
        selectionAnchor_ = caretIndex();
    }
    caretIndex_ = next;
    if (extendSelection) {
        hasSelection_ = selectionAnchor_ != caretIndex_;
    } else {
        hasSelection_ = false;
        selectionAnchor_ = caretIndex_;
    }
    ensureCaretVisible();
    restartCaretBlink();
    invalidate();
}

bool TextField::deleteSelection() {
    if (!editable() || !hasSelection()) {
        return false;
    }

    std::wstring next = value();
    const std::size_t start = selectionStart();
    next.erase(start, selectionEnd() - start);
    assignText(std::move(next), start, true);
    return true;
}

const std::wstring& TextField::value() const {
    return textBinding_.get(text_);
}

std::wstring TextField::displayText() const {
    if (!passwordMode_) {
        return value();
    }
    return std::wstring(value().size(), passwordMask_);
}

TextFieldStyle TextField::resolvedStyle() const {
    TextFieldStyle style = baseTextFieldStyle(disabled(), readOnly_, hovered_);
    if (!styleOverride_) {
        return style;
    }

    if (styleOverride_->normal) {
        applyTextFieldStateOverride(style, *styleOverride_->normal);
    }
    if (disabled() && styleOverride_->disabled) {
        applyTextFieldStateOverride(style, *styleOverride_->disabled);
    } else if (readOnly_ && styleOverride_->readOnly) {
        applyTextFieldStateOverride(style, *styleOverride_->readOnly);
    } else if (hovered_ && styleOverride_->hovered) {
        applyTextFieldStateOverride(style, *styleOverride_->hovered);
    }
    if (editable() && (focused() || focusVisible()) && styleOverride_->focusVisible) {
        applyTextFieldStateOverride(style, *styleOverride_->focusVisible);
    }
    return style;
}

TextFieldStyle TextField::visualStyle(TextFieldStyle target) const {
    if (!visualInitialized_) {
        return target;
    }

    target.background = backgroundTransition_.value();
    target.foreground = foregroundTransition_.value();
    target.placeholderForeground = placeholderTransition_.value();
    target.border = borderTransition_.value();
    return target;
}

void TextField::beginVisualTransition(TextFieldStyle from, TextFieldStyle target) {
    if (!hasAnimationScheduler()) {
        visualInitialized_ = false;
        return;
    }

    if (!visualInitialized_) {
        backgroundTransition_.reset(from.background);
        foregroundTransition_.reset(from.foreground);
        placeholderTransition_.reset(from.placeholderForeground);
        borderTransition_.reset(from.border);
        visualInitialized_ = true;
    }

    const double nowMs = currentTimeMs();
    backgroundTransition_.animateTo(target.background, nowMs, target.transition);
    foregroundTransition_.animateTo(target.foreground, nowMs, target.transition);
    placeholderTransition_.animateTo(target.placeholderForeground, nowMs, target.transition);
    borderTransition_.animateTo(target.border, nowMs, target.transition);
    if (backgroundTransition_.running() || foregroundTransition_.running() || placeholderTransition_.running() || borderTransition_.running()) {
        requestAnimationFrame();
    }
}

void TextField::restartCaretBlink() {
    caretBlinkStartMs_ = currentTimeMs();
    caretBlinkVisible_ = true;
    invalidate();
    if (focused() && editable()) {
        requestAnimationFrame();
    }
}

std::size_t TextField::caretIndexFromPoint(Point point) const {
    if (multiline_) {
        return multilineCaretIndexFromPoint(point);
    }
    const TextFieldStyle style = resolvedStyle();
    const float prefixOffset = prefixIcon_ ? AffixIconSize + AffixIconGap : 0.0f;
    const float localX = std::max(0.0f, point.x - frame().x - style.padding.left - prefixOffset) + textWidthAt(textScrollOffset_);
    updateTextMetrics();
    if (measuredPrefixWidths_.empty()) {
        return 0;
    }

    auto it = std::lower_bound(measuredPrefixWidths_.begin(), measuredPrefixWidths_.end(), localX);
    if (it == measuredPrefixWidths_.begin()) {
        return 0;
    }
    if (it == measuredPrefixWidths_.end()) {
        return value().size();
    }

    const std::size_t upper = static_cast<std::size_t>(it - measuredPrefixWidths_.begin());
    const std::size_t lower = upper - 1;
    const float lowerDistance = std::fabs(localX - measuredPrefixWidths_[lower]);
    const float upperDistance = std::fabs(measuredPrefixWidths_[upper] - localX);
    return lowerDistance <= upperDistance ? lower : upper;
}

std::size_t TextField::multilineCaretIndexFromPoint(Point point) const {
    updateMultilineTextMetrics();
    if (measuredLines_.empty()) {
        return 0;
    }

    const TextFieldStyle style = resolvedStyle();
    const float prefixOffset = prefixIcon_ ? AffixIconSize + AffixIconGap : 0.0f;
    const float localY = std::max(0.0f, point.y - frame().y - style.padding.top);
    const std::size_t relativeLine = static_cast<std::size_t>(localY / lineHeight_);
    const std::size_t lineIndex = std::min(
        verticalScrollLine_ + relativeLine,
        measuredLines_.size() - 1);
    const auto& widths = measuredLinePrefixWidths_[lineIndex];
    const float localX = std::max(
        0.0f,
        point.x - frame().x - style.padding.left - prefixOffset + horizontalScrollOffset_);

    auto it = std::lower_bound(widths.begin(), widths.end(), localX);
    std::size_t column = 0;
    if (it == widths.end()) {
        column = widths.size() - 1;
    } else if (it != widths.begin()) {
        const std::size_t upper = static_cast<std::size_t>(it - widths.begin());
        const std::size_t lower = upper - 1;
        column = std::fabs(localX - widths[lower]) <= std::fabs(widths[upper] - localX)
            ? lower
            : upper;
    }
    return measuredLines_[lineIndex].start + column;
}

bool TextField::moveCaretVertically(int direction, bool extendSelection) {
    updateMultilineTextMetrics();
    if (measuredLines_.empty() || direction == 0) {
        return false;
    }

    const std::size_t currentLine = multilineLineIndexForCaret(caretIndex());
    const std::size_t targetLine = direction < 0
        ? (currentLine == 0 ? 0 : currentLine - 1)
        : std::min(currentLine + 1, measuredLines_.size() - 1);
    if (targetLine == currentLine) {
        return false;
    }

    const std::size_t column = caretIndex() - measuredLines_[currentLine].start;
    const TextLine& target = measuredLines_[targetLine];
    setCaretIndexInternal(target.start + std::min(column, target.end - target.start), extendSelection);
    return true;
}

void TextField::ensureCaretVisible() {
    if (multiline_) {
        ensureMultilineCaretVisible();
        return;
    }
    updateTextMetrics();
    const std::size_t size = value().size();
    textScrollOffset_ = std::min(textScrollOffset_, size);
    if (measuredPrefixWidths_.empty()) {
        textScrollOffset_ = 0;
        return;
    }
    if (!focused() && !selecting_) {
        textScrollOffset_ = 0;
        return;
    }

    const float contentWidth = contentWidthForText();
    if (contentWidth <= 0.0f) {
        textScrollOffset_ = std::min(caretIndex(), size);
        return;
    }

    const float scrollX = textWidthAt(textScrollOffset_);
    const std::size_t caret = caretIndex();
    const float caretX = textWidthAt(caret);
    if (caretX < scrollX) {
        textScrollOffset_ = caret;
    } else if (caretX - scrollX > contentWidth) {
        const float targetX = caretX - contentWidth;
        auto it = std::lower_bound(
            measuredPrefixWidths_.begin(),
            measuredPrefixWidths_.begin() + static_cast<std::ptrdiff_t>(caret) + 1,
            targetX);
        textScrollOffset_ = static_cast<std::size_t>(it - measuredPrefixWidths_.begin());
    }
}

void TextField::ensureMultilineCaretVisible() {
    updateMultilineTextMetrics();
    if (measuredLines_.empty()) {
        verticalScrollLine_ = 0;
        horizontalScrollOffset_ = 0.0f;
        return;
    }
    if (!focused() && !selecting_) {
        verticalScrollLine_ = 0;
        horizontalScrollOffset_ = 0.0f;
        return;
    }

    const std::size_t lineIndex = multilineLineIndexForCaret(caretIndex());
    const float contentHeight = std::max(0.0f, frame().height - resolvedStyle().padding.vertical());
    const std::size_t visibleLines = std::max<std::size_t>(
        1,
        static_cast<std::size_t>(std::floor(contentHeight / lineHeight_)));
    if (lineIndex < verticalScrollLine_) {
        verticalScrollLine_ = lineIndex;
    } else if (lineIndex >= verticalScrollLine_ + visibleLines) {
        verticalScrollLine_ = lineIndex - visibleLines + 1;
    }

    const float contentWidth = contentWidthForText();
    const float caretX = multilineTextWidthAt(lineIndex, caretIndex());
    if (caretX < horizontalScrollOffset_) {
        horizontalScrollOffset_ = caretX;
    } else if (caretX - horizontalScrollOffset_ > contentWidth) {
        horizontalScrollOffset_ = std::max(0.0f, caretX - contentWidth + CaretWidth + 2.0f);
    }
}

float TextField::contentWidthForText() const {
    const TextFieldStyle style = resolvedStyle();
    const float affixWidth = (prefixIcon_ ? AffixIconSize + AffixIconGap : 0.0f) +
        (suffixIcon_ ? AffixIconSize + AffixIconGap : 0.0f);
    return std::max(0.0f, frame().width - style.padding.horizontal() - affixWidth);
}

void TextField::paintMultilineContent(
    Canvas& canvas,
    Rect contentRect,
    const TextFieldStyle& style,
    bool hasText,
    bool shouldPaintPlaceholder) {
    updateMultilineTextMetrics(&canvas);
    ensureMultilineCaretVisible();

    canvas.save();
    canvas.clipRect(contentRect);

    if (!hasText && shouldPaintPlaceholder) {
        canvas.drawText(
            placeholder_,
            Rect{contentRect.x, contentRect.y, contentRect.width, lineHeight_},
            style.placeholderForeground,
            theme().fontMd,
            TextAlign::Left);
    }

    const std::wstring display = displayText();
    const std::size_t firstLine = std::min(verticalScrollLine_, measuredLines_.size() - 1);
    const std::size_t visibleLines = std::max<std::size_t>(
        1,
        static_cast<std::size_t>(std::ceil(contentRect.height / lineHeight_)) + 1);
    const std::size_t lastLine = std::min(measuredLines_.size(), firstLine + visibleLines);

    for (std::size_t lineIndex = firstLine; lineIndex < lastLine; ++lineIndex) {
        const TextLine& line = measuredLines_[lineIndex];
        const float y = contentRect.y + static_cast<float>(lineIndex - firstLine) * lineHeight_;
        const Rect lineRect{
            contentRect.x - horizontalScrollOffset_,
            y,
            contentRect.width + horizontalScrollOffset_,
            lineHeight_};

        if (hasSelection()) {
            const std::size_t start = std::max(selectionStart(), line.start);
            const std::size_t end = std::min(selectionEnd(), line.end);
            if (end > start ||
                (selectionEnd() > line.end && selectionStart() <= line.end && lineIndex + 1 < measuredLines_.size())) {
                const float selectionX = contentRect.x + multilineTextWidthAt(lineIndex, start) - horizontalScrollOffset_;
                float selectionWidth = multilineTextWidthAt(lineIndex, end) - multilineTextWidthAt(lineIndex, start);
                if (selectionEnd() > line.end && lineIndex + 1 < measuredLines_.size()) {
                    selectionWidth += std::max(4.0f, theme().fontMd * 0.45f);
                }
                canvas.fillRect(
                    Rect{selectionX, y + 2.0f, selectionWidth, std::max(0.0f, lineHeight_ - 4.0f)},
                    style.selectionBackground,
                    3.0f);
            }
        }

        if (line.end > line.start) {
            canvas.drawText(
                display.substr(line.start, line.end - line.start),
                lineRect,
                style.foreground,
                theme().fontMd,
                TextAlign::Left);
        }
    }

    if (focused() && editable() && caretBlinkVisible_) {
        const std::size_t lineIndex = multilineLineIndexForCaret(caretIndex());
        if (lineIndex >= firstLine && lineIndex < lastLine) {
            float caretX = contentRect.x + multilineTextWidthAt(lineIndex, caretIndex()) - horizontalScrollOffset_;
            caretX = std::floor(caretX) + 0.5f;
            const float caretHeight = std::min(CaretMaxHeight, std::max(CaretMinHeight, lineHeight_ - 6.0f));
            const float lineY = contentRect.y + static_cast<float>(lineIndex - firstLine) * lineHeight_;
            const float caretY = lineY + (lineHeight_ - caretHeight) * 0.5f;
            canvas.fillRect(Rect{caretX, caretY, CaretWidth, caretHeight}, style.caretColor, 0.5f);
        }
    }

    canvas.restore();
}

void TextField::invalidateTextMetrics() {
    measuredDisplayText_.clear();
    measuredPrefixWidths_.clear();
    measuredTextMetricsExact_ = false;
    measuredLines_.clear();
    measuredLinePrefixWidths_.clear();
    measuredMultilineMetricsExact_ = false;
}

void TextField::updateTextMetrics(const Canvas* canvas) const {
    const std::wstring display = displayText();
    if (!measuredPrefixWidths_.empty() &&
        measuredDisplayText_ == display &&
        (measuredTextMetricsExact_ || canvas == nullptr)) {
        return;
    }

    measuredDisplayText_ = display;
    measuredPrefixWidths_.assign(display.size() + 1, 0.0f);
    if (canvas) {
        measuredPrefixWidths_ = canvas->measureTextPrefixWidths(display, theme().fontMd);
        measuredTextMetricsExact_ = true;
        return;
    }

    for (std::size_t index = 0; index < display.size(); ++index) {
        measuredPrefixWidths_[index + 1] = measuredPrefixWidths_[index] + approximateGlyphWidth(display[index]);
    }
    measuredTextMetricsExact_ = false;
}

float TextField::textWidthAt(std::size_t index) const {
    updateTextMetrics();
    if (measuredPrefixWidths_.empty()) {
        return 0.0f;
    }
    return measuredPrefixWidths_[std::min(index, measuredPrefixWidths_.size() - 1)];
}

void TextField::updateMultilineTextMetrics(const Canvas* canvas) const {
    const std::wstring display = displayText();
    if (!measuredLines_.empty() && measuredDisplayText_ == display &&
        (measuredMultilineMetricsExact_ || canvas == nullptr)) {
        return;
    }

    measuredDisplayText_ = display;
    measuredLines_.clear();
    measuredLinePrefixWidths_.clear();

    std::size_t lineStart = 0;
    for (std::size_t index = 0; index <= display.size(); ++index) {
        if (index != display.size() && display[index] != L'\n') {
            continue;
        }
        measuredLines_.push_back(TextLine{lineStart, index});
        lineStart = index + 1;
    }
    if (measuredLines_.empty()) {
        measuredLines_.push_back(TextLine{});
    }

    measuredLinePrefixWidths_.reserve(measuredLines_.size());
    for (const TextLine& line : measuredLines_) {
        std::vector<float> widths(line.end - line.start + 1, 0.0f);
        if (canvas) {
            widths = canvas->measureTextPrefixWidths(
                display.substr(line.start, line.end - line.start),
                theme().fontMd);
        } else {
            for (std::size_t index = line.start; index < line.end; ++index) {
                widths[index - line.start + 1] = widths[index - line.start] + approximateGlyphWidth(display[index]);
            }
        }
        measuredLinePrefixWidths_.push_back(std::move(widths));
    }
    measuredMultilineMetricsExact_ = canvas != nullptr;
}

float TextField::multilineTextWidthAt(std::size_t lineIndex, std::size_t index) const {
    updateMultilineTextMetrics();
    if (measuredLines_.empty()) {
        return 0.0f;
    }
    lineIndex = std::min(lineIndex, measuredLines_.size() - 1);
    const TextLine& line = measuredLines_[lineIndex];
    const std::size_t column = std::clamp(index, line.start, line.end) - line.start;
    const auto& widths = measuredLinePrefixWidths_[lineIndex];
    return widths[std::min(column, widths.size() - 1)];
}

std::size_t TextField::multilineLineIndexForCaret(std::size_t index) const {
    updateMultilineTextMetrics();
    const std::size_t caret = std::min(index, value().size());
    for (std::size_t line = 0; line < measuredLines_.size(); ++line) {
        if (caret <= measuredLines_[line].end) {
            return line;
        }
    }
    return measuredLines_.empty() ? 0 : measuredLines_.size() - 1;
}

bool TextField::hasInteractionState() const {
    return hovered_ || selecting_;
}

void TextField::resetInteractionState() {
    hovered_ = false;
    selecting_ = false;
    const TextFieldStyle target = resolvedStyle();
    backgroundTransition_.reset(target.background);
    foregroundTransition_.reset(target.foreground);
    placeholderTransition_.reset(target.placeholderForeground);
    borderTransition_.reset(target.border);
    visualInitialized_ = true;
}

} // namespace oneui

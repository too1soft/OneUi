#include "oneui/controls/text_field.h"

#include "oneui/icon.h"
#include "oneui/style.h"
#include "internal/unicode.h"
#include "text/text_layout.h"

#include <algorithm>
#include "internal/ui_clock.h"
#include <cmath>
#include <utility>

namespace oneui {
namespace {


constexpr float AffixIconSize = 14.0f;
constexpr float AffixIconGap = 8.0f;
constexpr float CaretWidth = 1.0f;
constexpr float CaretMinHeight = 11.0f;
constexpr float CaretMaxHeight = 14.0f;
constexpr float CaretVisualInset = 1.0f;
constexpr double CaretBlinkPeriodMs = 1060.0;
constexpr double CaretBlinkOnMs = 530.0;



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
    return internal::uiTimeMs();
}

} // namespace

struct TextField::TextLayoutState {
    std::wstring value, composition, family, presented, displayed;
    std::size_t replaceStart = 0, replaceEnd = 0;
    float width = 0, size = 0, lineHeight = 0, scale = 1;
    bool password = false;
    wchar_t mask = L'*';
    TextOptions options;
    std::shared_ptr<text::Layout> logical, presentedLayout, display;
};

TextField::~TextField() = default;

CommandResult TextField::queryBuiltinCommand(const std::string& id) const {
    bool enabled = false;
    if (id == "edit.select_all") enabled = interactive() && !value().empty();
    else if (id == "edit.copy") enabled = interactive() && clipboard_ && hasSelection() && !passwordMode_;
    else if (id == "edit.cut") enabled = editable() && clipboard_ && hasSelection() && !passwordMode_;
    else if (id == "edit.paste") enabled = editable() && clipboard_;
    else if (id == "edit.undo") enabled = editable() && !undoStack_.empty();
    else if (id == "edit.redo") enabled = editable() && !redoStack_.empty();
    else return CommandResult::NotFound;
    return enabled ? CommandResult::Enabled : CommandResult::Disabled;
}
CommandResult TextField::executeBuiltinCommand(const std::string& id) {
    const auto state = queryBuiltinCommand(id);
    if (state != CommandResult::Enabled) return state;
    if (id == "edit.select_all") selectAll();
    else if (id == "edit.copy") copySelectionToClipboard(*clipboard_);
    else if (id == "edit.cut") cutSelectionToClipboard(*clipboard_);
    else if (id == "edit.paste") pasteFromClipboard(*clipboard_);
    else if (id == "edit.undo") undo();
    else if (id == "edit.redo") redo();
    return CommandResult::Executed;
}
CommandResult TextField::dispatchBuiltinCommandKey(const KeyEvent& event, const std::string& logicalKey) {
    if (!event.pressed || !event.editShortcut() || event.alt || hasTextComposition()) return CommandResult::NotFound;
#ifdef __APPLE__
    if (event.control) return CommandResult::NotFound;
#else
    if (event.win) return CommandResult::NotFound;
#endif
    auto key = logicalKey.empty() ? logicalKeyName(event) : logicalKey;
    for (auto& c : key) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
    if (key == "z") return executeBuiltinCommand(event.shift ? "edit.redo" : "edit.undo");
#ifndef __APPLE__
    if (key == "y" && !event.shift) return executeBuiltinCommand("edit.redo");
#endif
    if (event.shift) return CommandResult::NotFound;
    if (key == "a") return executeBuiltinCommand("edit.select_all");
    if (key == "c") return executeBuiltinCommand("edit.copy");
    if (key == "x") return executeBuiltinCommand("edit.cut");
    if (key == "v") return executeBuiltinCommand("edit.paste");
    return CommandResult::NotFound;
}

void TextField::setTextOptions(TextOptions options) {
    if (options.locale.empty()) options.locale = "und";
    textOptions_ = std::move(options);
    invalidateTextMetrics();
    ensureCaretVisible();
    invalidate();
}

void TextField::ensureTextLayout() const {
    const float width = contentWidthForText();
    const float height = multiline_ ? lineHeight_ : 0;
    const auto start = composition_.empty() ? 0 : selectionStart();
    const auto end = composition_.empty() ? 0 : selectionEnd();
    if (textLayout_ && textLayout_->value == value() && textLayout_->composition == composition_ &&
        textLayout_->replaceStart == start && textLayout_->replaceEnd == end &&
        textLayout_->family == textFontFamily() && textLayout_->scale == textDpiScale() &&
        textLayout_->width == width && textLayout_->size == fontSize_ && textLayout_->lineHeight == height &&
        textLayout_->password == passwordMode_ && textLayout_->mask == passwordMask_ &&
        textLayout_->options.direction == textOptions_.direction && textLayout_->options.wrap == textOptions_.wrap &&
        textLayout_->options.locale == textOptions_.locale) return;
    auto state = std::make_unique<TextLayoutState>();
    state->value = value(); state->composition = composition_; state->family = textFontFamily();
    state->width = width; state->size = fontSize_; state->lineHeight = height; state->scale = textDpiScale();
    state->replaceStart = start; state->replaceEnd = end; state->options = textOptions_;
    state->password = passwordMode_; state->mask = passwordMask_;
    text::LayoutOptions options;
    options.text = textOptions_;
    if (!multiline_) options.text.wrap = TextWrapMode::NoWrap;
    options.width = options.text.wrap == TextWrapMode::WordWrap ? width : 1000000.0f;
    options.size = state->size; options.lineHeight = height; options.family = state->family;
    options.scale = state->scale; options.sensitive = passwordMode_;
    state->logical = text::Layout::make(state->value, options);
    state->presented = state->value;
    if (!composition_.empty()) state->presented.replace(start, end - start, composition_);
    state->presentedLayout = state->presented == state->value ? state->logical : text::Layout::make(state->presented, options);
    state->displayed = passwordMode_ ? std::wstring(state->presentedLayout->graphemes().size() - 1, passwordMask_) : state->presented;
    if (passwordMode_) {
        options.text.direction = TextDirection::LTR;
        state->display = text::Layout::make(state->displayed, options);
    } else state->display = state->presentedLayout;
    textLayout_ = std::move(state);
}

std::size_t TextField::toDisplayOffset(std::size_t offset) const {
    ensureTextLayout();
    offset = textLayout_->logical->floor(offset);
    if (!composition_.empty() && offset > textLayout_->replaceStart) {
        offset = offset >= textLayout_->replaceEnd ?
            offset - (textLayout_->replaceEnd - textLayout_->replaceStart) + composition_.size() : textLayout_->replaceStart;
    }
    if (!passwordMode_) return offset;
    const auto& boundaries = textLayout_->presentedLayout->graphemes();
    return static_cast<std::size_t>(std::lower_bound(boundaries.begin(), boundaries.end(), offset) - boundaries.begin());
}

std::size_t TextField::fromDisplayOffset(std::size_t offset) const {
    ensureTextLayout();
    if (passwordMode_) {
        const auto& boundaries = textLayout_->presentedLayout->graphemes();
        offset = boundaries[std::min(offset, boundaries.size() - 1)];
    }
    if (!composition_.empty() && offset > textLayout_->replaceStart) {
        const auto compositionEnd = textLayout_->replaceStart + composition_.size();
        offset = offset >= compositionEnd ?
            offset - composition_.size() + (textLayout_->replaceEnd - textLayout_->replaceStart) : textLayout_->replaceStart;
    }
    return textLayout_->logical->floor(offset);
}

std::size_t TextField::displayCaretOffset() const {
    ensureTextLayout();
    if (composition_.empty()) return toDisplayOffset(caretIndex());
    const auto offset = textLayout_->replaceStart + compositionCaret_;
    if (!passwordMode_) return textLayout_->presentedLayout->floor(offset);
    const auto& boundaries = textLayout_->presentedLayout->graphemes();
    return static_cast<std::size_t>(std::lower_bound(boundaries.begin(), boundaries.end(), offset) - boundaries.begin());
}

TextPosition TextField::textPosition() const {
    ensureTextLayout();
    return {textLayout_->logical->utf8Offset(caretIndex()), caretAffinity_};
}
bool TextField::setTextPosition(TextPosition position) {
    ensureTextLayout();
    const auto offset = textLayout_->logical->wideOffset(position.utf8Offset);
    if (textLayout_->logical->utf8Offset(offset) != position.utf8Offset ||
        textLayout_->logical->floor(offset) != offset) return false;
    setCaretIndexInternal(offset);
    caretAffinity_ = position.affinity;
    ensureCaretVisible(); invalidate();
    return true;
}

Point TextField::textOrigin(Rect content) const {
    ensureTextLayout();
    return {content.x - horizontalScrollOffset_,
            multiline_ ? content.y - verticalScrollOffset_ : content.y + (content.height - textLayout_->display->height()) / 2};
}

void TextField::paintTextContent(Canvas& canvas, Rect content, const TextFieldStyle& style, bool placeholder) {
    ensureTextLayout();
    ensureCaretVisible();
    const auto origin = textOrigin(content);
    const auto layout = textLayout_->display;
    canvas.save(); canvas.clipRect(content);
    if (hasSelection() && composition_.empty()) {
        for (auto box : layout->selection(toDisplayOffset(selectionStart()), toDisplayOffset(selectionEnd()))) {
            box.x += origin.x; box.y += origin.y;
            canvas.fillRect(box, style.selectionBackground, 2);
        }
    }
    TextBlockStyle textStyle;
    textStyle.dpiScale = textDpiScale();
    textStyle.options = textOptions_;
    if (!multiline_) textStyle.options.wrap = TextWrapMode::NoWrap;
    if (passwordMode_) textStyle.options.direction = TextDirection::LTR;
    textStyle.fontFamily = textFontFamily(); textStyle.fontSize = fontSize_;
    textStyle.lineHeight = multiline_ ? lineHeight_ : 0; textStyle.sensitive = passwordMode_;
    const Rect textRect{origin.x, origin.y, content.width, layout->height()};
    if (!textLayout_->displayed.empty()) {
        canvas.drawTextBlock(textLayout_->displayed, textRect, style.foreground, textStyle);
    } else if (placeholder) {
        canvas.drawTextBlock(placeholder_, textRect, style.placeholderForeground, textStyle);
    }
    if (!composition_.empty()) {
        std::size_t from = textLayout_->replaceStart, to = from + composition_.size();
        if (passwordMode_) {
            const auto& boundaries = textLayout_->presentedLayout->graphemes();
            from = static_cast<std::size_t>(std::lower_bound(boundaries.begin(), boundaries.end(), from) - boundaries.begin());
            to = static_cast<std::size_t>(std::lower_bound(boundaries.begin(), boundaries.end(), to) - boundaries.begin());
        }
        for (const auto box : layout->selection(from, to))
            canvas.drawLine({origin.x + box.x, origin.y + box.y + box.height - 1},
                            {origin.x + box.x + box.width, origin.y + box.y + box.height - 1}, style.caretColor, 1);
    }
    if (focused() && editable() && caretBlinkVisible_) {
        auto box = layout->caret({displayCaretOffset(), caretAffinity_});
        const float caretHeight = std::min(CaretMaxHeight, std::max(CaretMinHeight, fontSize_ + 1));
        box.x = std::floor(box.x + origin.x + CaretVisualInset) + 0.5f;
        box.y += origin.y + (box.height - caretHeight) / 2;
        box.height = caretHeight; box.width = CaretWidth;
        canvas.fillRect(box, style.caretColor, 0.5f);
    }
    canvas.restore();
}

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
    const auto alive = lifetimeToken();
    const std::size_t nextCaretIndex = text.size();
    const bool changed = assignText(std::move(text), nextCaretIndex);
    if (alive.expired()) return;
    if (!changed) {
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
    ensureTextLayout();
    selectionAnchor_ = textLayout_->logical->floor(std::min(start, size));
    caretIndex_ = textLayout_->logical->floor(std::min(end, size));
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
    if (!hasSelection() || passwordMode_) {
        return false;
    }
    try { clipboard.setText(selectedText()); } catch (...) { return false; }
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

    std::wstring pastedText;
    try { pastedText = clipboard.text(); } catch (...) { return false; }
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
    verticalScrollOffset_ = 0;
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

void TextField::setFontSize(float fontSize) {
    const float next = std::max(9.0f, fontSize);
    if (std::fabs(fontSize_ - next) < 0.01f) {
        return;
    }
    fontSize_ = next;
    invalidateTextMetrics();
    ensureCaretVisible();
    invalidate();
}

float TextField::fontSize() const {
    return fontSize_;
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
    if (enabled) text::Layout::clearCache(); // Do not retain a formerly public value when it becomes sensitive.
    invalidateTextMetrics();
    invalidate();
}

bool TextField::passwordMode() const {
    return passwordMode_;
}

void TextField::setPasswordMask(wchar_t mask) {
    const auto scalar = static_cast<std::uint32_t>(mask);
    if (scalar < 32 || scalar > 0x10FFFF || (scalar >= 0xD800 && scalar <= 0xDFFF)) return;
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
    paintTextContent(canvas, contentRect, style, shouldPaintPlaceholder);
}

bool TextField::onMouseMove(const MouseEvent& event) {
    if (!interactive()) {
        return false;
    }

    if (selecting_) {
        setCaretIndexInternal(caretIndexFromPoint(event.position), true);
        caretAffinity_ = hitAffinity_;
        ensureCaretVisible();
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
    setTextComposition({}, 0);
    selecting_ = true;
    const auto index = caretIndexFromPoint(event.position);
    setCaretIndexInternal(index, event.shift);
    caretAffinity_ = hitAffinity_;
    if (event.clickCount == 2) {
        const auto range = textLayout_->logical->word(index);
        setSelectionRange(range.first, range.second);
    } else if (event.clickCount >= 3) selectAll();
    ensureCaretVisible();
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

    if (dispatchBuiltinCommandKey(event, {}) != CommandResult::NotFound) return true;
    if (event.editShortcut()) return false;
    if (hasTextComposition()) return false;

    ensureTextLayout();
    if (event.key == Key::Left || event.key == Key::Right) {
        const int direction = event.key == Key::Left ? -1 : 1;
        auto current = text::Position{toDisplayOffset(caretIndex()), caretAffinity_};
        if (!event.shift && hasSelection()) {
            const auto a = textLayout_->display->caret({toDisplayOffset(selectionStart())});
            const auto b = textLayout_->display->caret({toDisplayOffset(selectionEnd())});
            const auto target = direction < 0 ? (a.x <= b.x ? selectionStart() : selectionEnd()) :
                                               (a.x >= b.x ? selectionStart() : selectionEnd());
            setCaretIndexInternal(target); return true;
        }
        const auto target = textLayout_->display->moveVisual(current, direction);
        setCaretIndexInternal(fromDisplayOffset(target.offset), event.shift);
        caretAffinity_ = target.affinity;
        ensureCaretVisible(); invalidate();
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
            const auto box = textLayout_->display->caret({toDisplayOffset(caretIndex()), caretAffinity_});
            target = fromDisplayOffset(textLayout_->display->hitTest({-1000000, box.y + box.height / 2}).offset);
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
            const auto box = textLayout_->display->caret({toDisplayOffset(caretIndex()), caretAffinity_});
            target = fromDisplayOffset(textLayout_->display->hitTest({1000000, box.y + box.height / 2}).offset);
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
            const auto callback = onSubmitted_;
            const auto submitted = value();
            callback(submitted);
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
        next.erase(removalIndex, textLayout_->logical->next(removalIndex) - removalIndex);
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
    const std::size_t removalIndex = textLayout_->logical->previous(caretIndex());
    next.erase(removalIndex, caretIndex() - removalIndex);
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
    return true;
}


bool TextField::onTextCommitted(const std::wstring& text) {
    if (!editable()) return false;
    setTextComposition({}, 0);
    std::wstring committed;
    for (std::size_t i = 0; i < text.size(); ++i) {
        wchar_t ch = text[i];
        if (ch == L'\r') {
            if (i + 1 < text.size() && text[i + 1] == L'\n') ++i;
            ch = L'\n';
        }
        if (ch == L'\n' && !multiline_) continue;
        if (ch < 32 && ch != L'\n' && ch != L'\t') continue;
        committed += ch;
    }
    return replaceTextRange(selectionStart(), selectionEnd(), committed);
}
TextInputState TextField::textInputState() const {
    return {passwordMode_ ? std::wstring{} : value(), passwordMode_ ? 0 : (hasSelection() ? selectionAnchor_ : caretIndex()),
            passwordMode_ ? 0 : caretIndex(), editable(), passwordMode_, this, true, textInputSession()};
}
bool TextField::replaceTextRange(std::size_t start, std::size_t end, const std::wstring& text) {
    if (!editable()) return false;
    setTextComposition({}, 0);
    start = unicode::boundary(value(), start);
    end = unicode::boundary(value(), end);
    if (end < start) std::swap(start, end);
    std::wstring next = value();
    next.replace(start, end - start, text);
    return assignText(std::move(next), start + text.size(), true);
}
void TextField::setTextComposition(std::wstring text, std::size_t caret) {
    if (!editable() && !text.empty()) return;
    composition_ = std::move(text);
    compositionCaret_ = unicode::boundary(composition_, caret);
    invalidateTextMetrics();
    invalidate();
}
Rect TextField::textInputCaretRect() const {
    ensureTextLayout();
    auto content = frame().inset(resolvedStyle().padding);
    if (prefixIcon_) content.x += AffixIconSize + AffixIconGap;
    const auto origin = textOrigin(content);
    auto result = textLayout_->display->caret({displayCaretOffset(), caretAffinity_});
    result.x += origin.x; result.y += origin.y;
    return result;
}

bool TextField::onFocusChanged(bool focused) {
    if (!focused) setTextComposition({}, 0);
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
    const auto alive = lifetimeToken();
    const std::wstring previous = value();
    if (text == previous) {
        return false;
    }

    const TextEditSnapshot before = makeEditSnapshot();

    applyingInternalTextChange_ = true;
    textBinding_.set(std::move(text), text_);
    if (alive.expired()) return true;
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
    restartCaretBlink();
    const auto callback = onChanged_;
    if (callback) callback(current);
    return true;
}

bool TextField::editable() const {
    return interactive() && !readOnly_;
}

TextField::TextEditSnapshot TextField::makeEditSnapshot() const {
    return TextEditSnapshot{value(), caretIndex(), selectionAnchor_, hasSelection()};
}

bool TextField::restoreEditSnapshot(const TextEditSnapshot& snapshot) {
    const auto alive = lifetimeToken();
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
    if (alive.expired()) return true;
    applyingInternalTextChange_ = false;
    text_ = value();
    invalidateTextMetrics();
    caretIndex_ = std::min(snapshot.caretIndex, text_.size());
    selectionAnchor_ = std::min(snapshot.selectionAnchor, text_.size());
    hasSelection_ = snapshot.hasSelection && selectionAnchor_ != caretIndex_;
    ensureCaretVisible();
    invalidate();

    const std::wstring current = text_;
    const auto callback = onChanged_;
    if (callback) callback(current);
    return true;
}

void TextField::clearEditHistory() {
    undoStack_.clear();
    redoStack_.clear();
}

void TextField::clampCaret() {
    ensureTextLayout();
    const std::size_t next = textLayout_->logical->floor(caretIndex_);
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
    ensureTextLayout();
    selectionAnchor_ = textLayout_->logical->floor(selectionAnchor_);
    caretIndex_ = textLayout_->logical->floor(caretIndex_);
    hasSelection_ = selectionAnchor_ != caretIndex_;
    ensureCaretVisible();
}

void TextField::setCaretIndexInternal(std::size_t index, bool extendSelection) {
    ensureTextLayout();
    const std::size_t next = textLayout_->logical->floor(index);
    verticalCaretX_.reset();
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
    ensureTextLayout();
    return passwordMode_ ? std::wstring(textLayout_->logical->graphemes().size() - 1, passwordMask_) : value();
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
    ensureTextLayout();
    auto content = frame().inset(resolvedStyle().padding);
    if (prefixIcon_) content.x += AffixIconSize + AffixIconGap;
    const auto origin = textOrigin(content);
    const auto position = textLayout_->display->hitTest({point.x - origin.x, point.y - origin.y});
    hitAffinity_ = position.affinity;
    return fromDisplayOffset(position.offset);
}



bool TextField::moveCaretVertically(int direction, bool extendSelection) {
    ensureTextLayout();
    const auto position = text::Position{toDisplayOffset(caretIndex()), caretAffinity_};
    const float x = verticalCaretX_.value_or(textLayout_->display->caret(position).x);
    const auto target = textLayout_->display->moveVertical(position, direction, x);
    setCaretIndexInternal(fromDisplayOffset(target.offset), extendSelection);
    caretAffinity_ = target.affinity; verticalCaretX_ = x;
    ensureCaretVisible(); invalidate();
    return true;
}

void TextField::ensureCaretVisible() {
    ensureTextLayout();
    if (!focused() && !selecting_) { horizontalScrollOffset_ = 0; verticalScrollOffset_ = 0; return; }
    const auto caret = textLayout_->display->caret({displayCaretOffset(), caretAffinity_});
    const float width = contentWidthForText();
    const float height = std::max(0.0f, frame().height - resolvedStyle().padding.vertical());
    if (caret.x < horizontalScrollOffset_) horizontalScrollOffset_ = caret.x;
    else if (caret.x + CaretWidth + 2 * CaretVisualInset > horizontalScrollOffset_ + width)
        horizontalScrollOffset_ = std::max(0.0f, caret.x + CaretWidth + 2 * CaretVisualInset - width);
    if (multiline_) {
        if (caret.y < verticalScrollOffset_) verticalScrollOffset_ = caret.y;
        else if (caret.y + caret.height > verticalScrollOffset_ + height)
            verticalScrollOffset_ = std::max(0.0f, caret.y + caret.height - height);
    }
}



float TextField::contentWidthForText() const {
    const TextFieldStyle style = resolvedStyle();
    const float affixWidth = (prefixIcon_ ? AffixIconSize + AffixIconGap : 0.0f) +
        (suffixIcon_ ? AffixIconSize + AffixIconGap : 0.0f);
    return std::max(0.0f, frame().width - style.padding.horizontal() - affixWidth);
}



void TextField::invalidateTextMetrics() { textLayout_.reset(); }











bool TextField::hasInteractionState() const {
    return hovered_ || selecting_ || !composition_.empty();
}

void TextField::resetInteractionState() {
    composition_.clear();
    compositionCaret_ = 0;
    invalidateTextMetrics();
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

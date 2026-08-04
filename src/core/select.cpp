#include "oneui/controls/select.h"

#include "oneui/controls/popup.h"
#include "oneui/style.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace oneui {
namespace {

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

void applySelectStateOverride(SelectStyle& style, const SelectStateStyleOverride& override) {
    if (override.background) {
        style.background = *override.background;
    }
    if (override.foreground) {
        style.foreground = *override.foreground;
    }
    if (override.border) {
        style.border = *override.border;
    }
    if (override.arrowColor) {
        style.arrowColor = *override.arrowColor;
    }
    if (override.popupBackground) {
        style.popupBackground = *override.popupBackground;
    }
    if (override.popupBorder) {
        style.popupBorder = *override.popupBorder;
    }
    if (override.popupShadow) {
        style.popupShadow = *override.popupShadow;
    }
    if (override.optionBackground) {
        style.optionBackground = *override.optionBackground;
    }
    if (override.optionForeground) {
        style.optionForeground = *override.optionForeground;
    }
    if (override.selectedOptionBackground) {
        style.selectedOptionBackground = *override.selectedOptionBackground;
    }
    if (override.selectedOptionForeground) {
        style.selectedOptionForeground = *override.selectedOptionForeground;
    }
    if (override.borderWidth) {
        style.borderWidth = *override.borderWidth;
    }
    if (override.radius) {
        style.radius = *override.radius;
    }
    if (override.popupRadius) {
        style.popupRadius = *override.popupRadius;
    }
    if (override.optionRadius) {
        style.optionRadius = *override.optionRadius;
    }
    if (override.popupOffset) {
        style.popupOffset = *override.popupOffset;
    }
    if (override.popupShadowOffset) {
        style.popupShadowOffset = *override.popupShadowOffset;
    }
    if (override.padding) {
        style.padding = *override.padding;
    }
    if (override.optionInset) {
        style.optionInset = *override.optionInset;
    }
    if (override.focusRing) {
        applyFocusRingOverride(style.focusRing, *override.focusRing);
    }
}

SelectStyle baseSelectStyle(bool disabled, bool hovered, bool pressed) {
    const auto& t = theme();
    SelectStyle style;
    style.background = disabled ? t.surfaceMuted : (pressed ? Color{241, 245, 249} : t.surface);
    style.foreground = disabled ? t.textSubtle : t.text;
    style.border = disabled ? t.border : (hovered ? t.borderStrong : t.border);
    style.arrowColor = disabled ? t.textSubtle : t.textMuted;
    style.popupBackground = t.surface;
    style.popupBorder = t.borderStrong;
    style.popupShadow = Color{148, 163, 184, 60};
    style.optionBackground = Color{0, 0, 0, 0};
    style.optionForeground = t.text;
    style.selectedOptionBackground = Color{219, 234, 254};
    style.selectedOptionForeground = t.primary;
    style.borderWidth = 1.0f;
    style.radius = t.radiusMd;
    style.popupRadius = t.radiusMd;
    style.optionRadius = 6.0f;
    style.popupOffset = 4.0f;
    style.popupShadowOffset = 2.0f;
    style.padding = Insets{0.0f, 12.0f};
    style.optionInset = Insets{2.0f, 4.0f};
    style.focusRing = FocusRingStyle{t.focusOutline, t.focusOutlineWidth, t.focusOutlineOffset, t.radiusLg, true};
    return style;
}

PopupOutsidePointerPolicy selectPopupOutsidePointerPolicy() {
    return PopupOutsidePointerPolicy::Close;
}

Rect selectPopupViewport(Rect anchor, Size preferredSize, float offset) {
    const float extent = std::numeric_limits<float>::max() / 8.0f;
    const float minimumX = std::min(anchor.x, anchor.x + anchor.width - preferredSize.width) - offset - extent;
    const float minimumY = std::min(anchor.y, anchor.y - preferredSize.height - offset) - extent;
    return Rect{minimumX, minimumY, extent * 2.0f, extent * 2.0f};
}

PopupPlacementRequest selectPopupPlacementRequest(Rect anchor, Size preferredSize, float offset) {
    return PopupPlacementRequest{
        anchor,
        preferredSize,
        selectPopupViewport(anchor, preferredSize, offset),
        PopupPreferredPlacement::BottomStart,
        offset
    };
}

} // namespace

Select::Select() {
    setPreferredSize(Size{180.0f, 36.0f});
}

void Select::setItems(std::vector<std::wstring> items) {
    const int previous = effectiveSelectedIndex();
    items_ = std::move(items);
    const int next = effectiveSelectedIndex();
    selectedBinding_.set(next, selectedIndex_);
    if (previous != next && onChanged_) {
        onChanged_(next);
    }
    if (items_.empty()) {
        closePopupSurface(PopupLightDismissReason::Unavailable);
    } else if (popup_.open) {
        popup_.hoveredIndex = effectiveSelectedIndex();
        popup_.pressedIndex = -1;
        popup_.fieldPressed = false;
    }
    invalidate();
}

void Select::setSelectedIndex(int index) {
    assignSelectedIndex(index);
}

int Select::selectedIndex() const {
    return selectedBinding_.get(selectedIndex_);
}

void Select::bindSelectedIndex(State<int>& state) {
    selectedBinding_ = Binding<int>(state, [this] {
        if (popup_.open) {
            popup_.hoveredIndex = effectiveSelectedIndex();
        }
        invalidate();
    });
    invalidate();
}

void Select::setStyleOverride(SelectStyleOverride style) {
    styleOverride_ = std::move(style);
    invalidate();
}

void Select::clearStyleOverride() {
    styleOverride_.reset();
    invalidate();
}

void Select::setOnChanged(std::function<void(int)> callback) {
    onChanged_ = std::move(callback);
}

void Select::paint(Canvas& canvas) {
    const Rect rect = frame();
    const SelectStyle fieldStyle = resolvedFieldStyle();

    if (focusVisible() && !disabled() && fieldStyle.focusRing.visible) {
        const float offset = fieldStyle.focusRing.offset;
        canvas.strokeRect(
            Rect{rect.x - offset, rect.y - offset, rect.width + offset * 2.0f, rect.height + offset * 2.0f},
            fieldStyle.focusRing.color,
            fieldStyle.focusRing.radius,
            fieldStyle.focusRing.width);
    }

    canvas.fillRect(rect, fieldStyle.background, fieldStyle.radius);
    canvas.strokeRect(rect, fieldStyle.border, fieldStyle.radius, fieldStyle.borderWidth);
    canvas.drawTextStyledEllipsized(
        selectedText(),
        Rect{
            rect.x + fieldStyle.padding.left,
            rect.y + fieldStyle.padding.top,
            std::max(0.0f, rect.width - fieldStyle.padding.horizontal() - 18.0f),
            std::max(0.0f, rect.height - fieldStyle.padding.vertical())},
        fieldStyle.foreground,
        theme().fontMd,
        TextAlign::Left);

    const float cx = rect.x + rect.width - 20.0f;
    const float cy = rect.y + rect.height / 2.0f + 1.0f;
    canvas.drawLine(Point{cx - 5.0f, cy - 3.0f}, Point{cx, cy + 2.0f}, fieldStyle.arrowColor, 1.5f);
    canvas.drawLine(Point{cx, cy + 2.0f}, Point{cx + 5.0f, cy - 3.0f}, fieldStyle.arrowColor, 1.5f);

    if (!popup_.open || !interactive() || items_.empty()) {
        return;
    }

    const Rect popup = popupSurfaceRect();
    canvas.fillRect(Rect{popup.x, popup.y + fieldStyle.popupShadowOffset, popup.width, popup.height}, fieldStyle.popupShadow, fieldStyle.popupRadius);
    canvas.fillRect(popup, fieldStyle.popupBackground, fieldStyle.popupRadius);
    canvas.strokeRect(popup, fieldStyle.popupBorder, fieldStyle.popupRadius, fieldStyle.borderWidth);

    const int selected = effectiveSelectedIndex();
    for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
        const Rect row = popupOptionRect(i);
        const SelectStyle optionStyle = resolvedOptionStyle(i);
        if (optionStyle.optionBackground.a > 0) {
            canvas.fillRect(row.inset(optionStyle.optionInset), optionStyle.optionBackground, optionStyle.optionRadius);
        }
        canvas.drawTextStyledEllipsized(
            items_[static_cast<std::size_t>(i)],
            Rect{row.x + optionStyle.padding.left, row.y + optionStyle.padding.top, std::max(0.0f, row.width - optionStyle.padding.horizontal()), std::max(0.0f, row.height - optionStyle.padding.vertical())},
            i == selected ? optionStyle.selectedOptionForeground : optionStyle.optionForeground,
            theme().fontMd,
            TextAlign::Left);
    }
}

bool Select::onMouseMove(const MouseEvent& event) {
    if (!interactive()) {
        closePopupSurface(PopupLightDismissReason::Unavailable);
        return false;
    }
    const bool next = contains(event.position);
    const int nextIndex = popup_.open ? hitPopupOptionIndex(event.position) : -1;
    if (next == hovered_ && nextIndex == popup_.hoveredIndex) {
        return false;
    }
    hovered_ = next;
    popup_.hoveredIndex = nextIndex;
    invalidate();
    return true;
}

bool Select::onMouseDown(const MouseEvent& event) {
    if (!interactive() || items_.empty()) {
        closePopupSurface(PopupLightDismissReason::Unavailable);
        return false;
    }

    if (popup_.open) {
        if (contains(event.position)) {
            popup_.fieldPressed = true;
            popup_.pressedIndex = -1;
            invalidate();
            return true;
        }

        const int index = hitPopupOptionIndex(event.position);
        if (index >= 0) {
            popup_.fieldPressed = true;
            popup_.pressedIndex = index;
            invalidate();
            return true;
        }

        if (selectPopupOutsidePointerPolicy() == PopupOutsidePointerPolicy::Close) {
            closePopupSurface(PopupLightDismissReason::OutsidePointer);
        }
        return false;
    }

    if (!contains(event.position)) {
        return false;
    }

    popup_.fieldPressed = true;
    popup_.pressedIndex = -1;
    invalidate();
    return true;
}

bool Select::onMouseUp(const MouseEvent& event) {
    if (!interactive()) {
        closePopupSurface(PopupLightDismissReason::Unavailable);
        return false;
    }
    const bool wasPressed = popup_.fieldPressed;
    const int wasPressedIndex = popup_.pressedIndex;
    popup_.fieldPressed = false;
    popup_.pressedIndex = -1;

    if (!wasPressed) {
        return false;
    }

    if (popup_.open) {
        const int index = hitPopupOptionIndex(event.position);
        if (wasPressedIndex >= 0 && index == wasPressedIndex) {
            assignSelectedIndex(index);
            closePopupSurface();
        } else if (wasPressedIndex < 0 && contains(event.position)) {
            closePopupSurface();
        }
        invalidate();
        return true;
    }

    if (contains(event.position) && !items_.empty()) {
        openPopupSurface();
    }

    invalidate();
    return wasPressed;
}

bool Select::onKeyDown(const KeyEvent& event) {
    if (!interactive() || items_.empty()) {
        closePopupSurface(PopupLightDismissReason::Unavailable);
        return false;
    }

    if (!popup_.open) {
        if (event.key == Key::Space || event.key == Key::Enter || event.key == Key::Down || event.key == Key::Up) {
            openPopupSurface();
            return true;
        }
        return false;
    }

    if (event.key == Key::Down) {
        popup_.hoveredIndex = std::min(static_cast<int>(items_.size()) - 1, std::max(0, popup_.hoveredIndex) + 1);
        invalidate();
        return true;
    }

    if (event.key == Key::Up) {
        popup_.hoveredIndex = std::max(0, (popup_.hoveredIndex < 0 ? effectiveSelectedIndex() : popup_.hoveredIndex) - 1);
        invalidate();
        return true;
    }

    if (event.key == Key::Escape) {
        closePopupSurface(PopupLightDismissReason::EscapeKey);
        invalidate();
        return true;
    }

    if (event.key == Key::Space || event.key == Key::Enter) {
        assignSelectedIndex(popup_.hoveredIndex < 0 ? effectiveSelectedIndex() : popup_.hoveredIndex);
        closePopupSurface();
        invalidate();
        return true;
    }

    return false;
}

bool Select::isFocusable() const {
    return interactive() && !items_.empty();
}

bool Select::onFocusChanged(bool focused) {
    Widget::onFocusChanged(focused);
    if (!focused || !interactive()) {
        closePopupSurface(PopupLightDismissReason::FocusLost);
    }
    return true;
}

bool Select::hitTest(Point point) const {
    if (!interactive()) {
        return false;
    }
    const bool inAnchor = contains(point);
    const bool inPopup = popup_.open && !items_.empty() && popupSurfaceRect().contains(point);
    const bool capturesOutside = popup_.open && !items_.empty() && selectPopupOutsidePointerPolicy() != PopupOutsidePointerPolicy::PassThrough;
    return inAnchor || inPopup || capturesOutside;
}

bool Select::paintsAboveSiblings() const {
    return interactive() && popup_.open && !items_.empty();
}

AccessibilityInfo Select::accessibilityInfo() const {
    auto info = Widget::accessibilityInfo();
    if (info.role == AccessibilityRole::None) {
        info.role = AccessibilityRole::ComboBox;
    }
    info.value = selectedText();
    info.state.expanded = interactive() && popup_.open && !items_.empty();
    return info;
}

void Select::assignSelectedIndex(int index) {
    if (items_.empty()) {
        const int previous = selectedIndex();
        selectedBinding_.set(0, selectedIndex_);
        invalidate();
        if (previous != 0 && onChanged_) {
            onChanged_(0);
        }
        return;
    }

    const int maxIndex = static_cast<int>(items_.size()) - 1;
    const int previous = std::clamp(selectedIndex(), 0, maxIndex);
    const int next = std::clamp(index, 0, maxIndex);

    selectedBinding_.set(next, selectedIndex_);
    invalidate();
    if (previous != next && onChanged_) {
        onChanged_(next);
    }
}

std::wstring Select::selectedText() const {
    if (items_.empty()) {
        return L"";
    }

    const int index = std::clamp(selectedIndex(), 0, static_cast<int>(items_.size()) - 1);
    return items_[static_cast<std::size_t>(index)];
}

SelectStyle Select::resolvedFieldStyle() const {
    SelectStyle style = baseSelectStyle(disabled(), hovered_, popup_.fieldPressed);
    if (!styleOverride_) {
        return style;
    }

    if (styleOverride_->normal) {
        applySelectStateOverride(style, *styleOverride_->normal);
    }
    if (disabled() && styleOverride_->disabled) {
        applySelectStateOverride(style, *styleOverride_->disabled);
    } else if (popup_.fieldPressed && styleOverride_->pressed) {
        applySelectStateOverride(style, *styleOverride_->pressed);
    } else if (hovered_ && styleOverride_->hovered) {
        applySelectStateOverride(style, *styleOverride_->hovered);
    }
    if (focusVisible() && styleOverride_->focusVisible) {
        applySelectStateOverride(style, *styleOverride_->focusVisible);
    }
    return style;
}

SelectStyle Select::resolvedOptionStyle(int index) const {
    const bool active = index == effectiveSelectedIndex();
    const bool hovered = index == popup_.hoveredIndex;
    const bool pressed = index == popup_.pressedIndex;
    SelectStyle style = baseSelectStyle(disabled(), hovered, pressed);
    if (active) {
        style.optionBackground = style.selectedOptionBackground;
    } else if (hovered) {
        style.optionBackground = Color{248, 250, 252};
    }

    if (!styleOverride_) {
        return style;
    }

    if (styleOverride_->normal) {
        applySelectStateOverride(style, *styleOverride_->normal);
    }
    if (active) {
        style.optionBackground = style.selectedOptionBackground;
    }
    if (disabled() && styleOverride_->disabled) {
        applySelectStateOverride(style, *styleOverride_->disabled);
    } else if (pressed && styleOverride_->pressed) {
        applySelectStateOverride(style, *styleOverride_->pressed);
    } else if (active && styleOverride_->selected) {
        applySelectStateOverride(style, *styleOverride_->selected);
        style.optionBackground = style.selectedOptionBackground;
    } else if (hovered && styleOverride_->hovered) {
        applySelectStateOverride(style, *styleOverride_->hovered);
    }
    if (focusVisible() && styleOverride_->focusVisible) {
        applySelectStateOverride(style, *styleOverride_->focusVisible);
    }
    return style;
}

bool Select::hasInteractionState() const {
    return hovered_ || popup_.open || popup_.fieldPressed || popup_.hoveredIndex >= 0 || popup_.pressedIndex >= 0;
}

void Select::resetInteractionState() {
    hovered_ = false;
    popup_ = LightDismissModel{};
}

float Select::popupRowHeight() const {
    const Rect rect = frame();
    return std::max(28.0f, rect.height);
}

Rect Select::popupSurfaceRect() const {
    const Rect rect = frame();
    const float rowHeight = popupRowHeight();
    const SelectStyle style = resolvedFieldStyle();
    const Size popupSize{rect.width, rowHeight * static_cast<float>(items_.size())};
    return PopupPlacement::resolve(selectPopupPlacementRequest(rect, popupSize, style.popupOffset)).rect;
}

Rect Select::popupOptionRect(int index) const {
    const Rect popup = popupSurfaceRect();
    const float rowHeight = popupRowHeight();
    return Rect{popup.x, popup.y + rowHeight * static_cast<float>(index), popup.width, rowHeight};
}

int Select::hitPopupOptionIndex(Point point) const {
    if (!popup_.open || items_.empty() || !popupSurfaceRect().contains(point)) {
        return -1;
    }
    for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
        if (popupOptionRect(i).contains(point)) {
            return i;
        }
    }
    return -1;
}

int Select::effectiveSelectedIndex() const {
    if (items_.empty()) {
        return 0;
    }
    return std::clamp(selectedIndex(), 0, static_cast<int>(items_.size()) - 1);
}

void Select::openPopupSurface() {
    if (!interactive() || items_.empty()) {
        closePopupSurface();
        return;
    }

    const bool changed = !popup_.open || popup_.hoveredIndex != effectiveSelectedIndex() || popup_.pressedIndex != -1 || popup_.fieldPressed;
    popup_.open = true;
    popup_.hoveredIndex = effectiveSelectedIndex();
    popup_.pressedIndex = -1;
    popup_.fieldPressed = false;
    if (changed) {
        invalidate();
    }
}

void Select::closePopupSurface(PopupLightDismissReason reason) {
    (void)reason;
    const bool changed = popup_.open || popup_.hoveredIndex != -1 || popup_.pressedIndex != -1 || popup_.fieldPressed;
    popup_ = LightDismissModel{};
    if (changed) {
        invalidate();
    }
}

} // namespace oneui

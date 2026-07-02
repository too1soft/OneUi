#include "oneui/controls/radio_group.h"

#include "oneui/style.h"

#include <algorithm>
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

void applyRadioGroupStateOverride(RadioGroupStyle& style, const RadioGroupStateStyleOverride& override) {
    if (override.itemBackground) {
        style.itemBackground = *override.itemBackground;
    }
    if (override.indicatorBackground) {
        style.indicatorBackground = *override.indicatorBackground;
    }
    if (override.indicatorBorder) {
        style.indicatorBorder = *override.indicatorBorder;
    }
    if (override.indicatorFill) {
        style.indicatorFill = *override.indicatorFill;
    }
    if (override.labelColor) {
        style.labelColor = *override.labelColor;
    }
    if (override.selectedLabelColor) {
        style.selectedLabelColor = *override.selectedLabelColor;
    }
    if (override.indicatorSize) {
        style.indicatorSize = *override.indicatorSize;
    }
    if (override.indicatorDotSize) {
        style.indicatorDotSize = *override.indicatorDotSize;
    }
    if (override.indicatorBorderWidth) {
        style.indicatorBorderWidth = *override.indicatorBorderWidth;
    }
    if (override.indicatorRadius) {
        style.indicatorRadius = *override.indicatorRadius;
    }
    if (override.itemRadius) {
        style.itemRadius = *override.itemRadius;
    }
    if (override.indicatorInset) {
        style.indicatorInset = *override.indicatorInset;
    }
    if (override.labelGap) {
        style.labelGap = *override.labelGap;
    }
    if (override.focusRing) {
        applyFocusRingOverride(style.focusRing, *override.focusRing);
    }
}

RadioGroupStyle baseRadioGroupStyle(bool selected, bool disabled, bool hovered, bool pressed) {
    const auto& t = theme();
    RadioGroupStyle style;
    style.itemBackground = (!disabled && hovered) ? Color{248, 250, 252} : Color{0, 0, 0, 0};
    style.indicatorBackground = disabled ? t.surfaceMuted : t.surface;
    style.indicatorBorder = disabled ? t.border : (selected ? t.primary : t.borderStrong);
    style.indicatorFill = disabled ? t.textSubtle : t.primary;
    style.labelColor = disabled ? t.textSubtle : t.textMuted;
    style.selectedLabelColor = style.labelColor;
    style.indicatorSize = 16.0f;
    style.indicatorDotSize = 8.0f;
    style.indicatorBorderWidth = 1.5f;
    style.indicatorRadius = 8.0f;
    style.itemRadius = t.radiusMd;
    style.indicatorInset = 8.0f;
    style.labelGap = 10.0f;
    style.focusRing = FocusRingStyle{t.focusOutline, t.focusOutlineWidth, t.focusOutlineOffset, t.radiusLg, true};

    if (!disabled && pressed) {
        style.itemBackground = Color{232, 236, 242};
        style.indicatorBorder = selected ? t.primaryPressed : t.borderStrong;
        style.indicatorFill = t.primaryPressed;
    }

    return style;
}

} // namespace

RadioGroup::RadioGroup() {
    setPreferredSize(Size{220.0f, 84.0f});
}

void RadioGroup::setItems(std::vector<std::wstring> items) {
    items_ = std::move(items);
    assignSelectedIndex(selectedIndex());
    invalidate();
}

void RadioGroup::setSelectedIndex(int index) {
    assignSelectedIndex(index);
}

int RadioGroup::selectedIndex() const {
    return selectedBinding_.get(selectedIndex_);
}

void RadioGroup::setOrientation(Orientation orientation) {
    if (orientation_ == orientation) {
        return;
    }
    orientation_ = orientation;
    invalidate();
}

RadioGroup::Orientation RadioGroup::orientation() const {
    return orientation_;
}

void RadioGroup::bindSelectedIndex(State<int>& state) {
    selectedBinding_ = Binding<int>(state, [this] {
        invalidate();
    });
    invalidate();
}

void RadioGroup::setStyleOverride(RadioGroupStyleOverride style) {
    styleOverride_ = std::move(style);
    invalidate();
}

void RadioGroup::clearStyleOverride() {
    styleOverride_.reset();
    invalidate();
}

void RadioGroup::setOnChanged(std::function<void(int)> callback) {
    onChanged_ = std::move(callback);
}

void RadioGroup::paint(Canvas& canvas) {
    const int selected = selectedIndex();
    const RadioGroupStyle groupStyle = resolvedStyle(selected);

    if (focusVisible() && !disabled() && groupStyle.focusRing.visible) {
        const float offset = groupStyle.focusRing.offset;
        canvas.strokeRect(
            Rect{frame().x - offset, frame().y - offset, frame().width + offset * 2.0f, frame().height + offset * 2.0f},
            groupStyle.focusRing.color,
            groupStyle.focusRing.radius,
            groupStyle.focusRing.width);
    }

    for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
        const Rect item = itemRect(i);
        const bool active = i == selected;
        const RadioGroupStyle style = resolvedStyle(i);
        if (style.itemBackground.a > 0) {
            canvas.fillRect(item, style.itemBackground, style.itemRadius);
        }

        const float indicatorSize = std::max(1.0f, style.indicatorSize);
        const float dotSize = std::max(1.0f, std::min(style.indicatorDotSize, indicatorSize));
        const Rect outer{item.x + style.indicatorInset, item.y + (item.height - indicatorSize) / 2.0f, indicatorSize, indicatorSize};
        canvas.fillEllipse(outer, style.indicatorBackground);
        canvas.strokeEllipse(outer, style.indicatorBorder, style.indicatorBorderWidth);
        if (active) {
            const float dotOffset = (indicatorSize - dotSize) / 2.0f;
            canvas.fillEllipse(Rect{outer.x + dotOffset, outer.y + dotOffset, dotSize, dotSize}, style.indicatorFill);
        }

        const float labelX = outer.x + indicatorSize + style.labelGap;
        canvas.drawText(
            items_[static_cast<std::size_t>(i)],
            Rect{labelX, item.y, std::max(0.0f, item.x + item.width - labelX), item.height},
            active ? style.selectedLabelColor : style.labelColor,
            theme().fontMd,
            TextAlign::Left);
    }
}

bool RadioGroup::onMouseMove(const MouseEvent& event) {
    if (!interactive()) {
        return false;
    }
    const int next = hitIndex(event.position);
    if (next == hoveredIndex_) {
        return false;
    }
    hoveredIndex_ = next;
    invalidate();
    return true;
}

bool RadioGroup::onMouseDown(const MouseEvent& event) {
    if (!interactive()) {
        return false;
    }
    const int index = hitIndex(event.position);
    if (index < 0) {
        return false;
    }
    pressedIndex_ = index;
    invalidate();
    return true;
}

bool RadioGroup::onMouseUp(const MouseEvent& event) {
    if (!interactive()) {
        return false;
    }
    const int wasPressed = pressedIndex_;
    pressedIndex_ = -1;
    const int index = hitIndex(event.position);
    if (wasPressed >= 0 && wasPressed == index) {
        assignSelectedIndex(index);
    }
    invalidate();
    return wasPressed >= 0;
}

bool RadioGroup::onKeyDown(const KeyEvent& event) {
    if (!interactive() || items_.empty()) {
        return false;
    }

    if (event.key == Key::Left || event.key == Key::Up) {
        assignSelectedIndex(selectedIndex() - 1);
        return true;
    }

    if (event.key == Key::Right || event.key == Key::Down) {
        assignSelectedIndex(selectedIndex() + 1);
        return true;
    }

    return false;
}

bool RadioGroup::isFocusable() const {
    return interactive() && !items_.empty();
}

AccessibilityInfo RadioGroup::accessibilityInfo() const {
    auto info = Widget::accessibilityInfo();
    if (info.role == AccessibilityRole::None) {
        info.role = AccessibilityRole::RadioGroup;
    }
    if (!items_.empty()) {
        const int index = std::clamp(selectedIndex(), 0, static_cast<int>(items_.size()) - 1);
        info.value = items_[static_cast<std::size_t>(index)];
        info.state.selected = true;
    }
    return info;
}

int RadioGroup::hitIndex(Point point) const {
    if (!frame().contains(point)) {
        return -1;
    }

    for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
        if (itemRect(i).contains(point)) {
            return i;
        }
    }
    return -1;
}

Rect RadioGroup::itemRect(int index) const {
    if (orientation_ == Orientation::Horizontal) {
        const float itemWidth = items_.empty() ? 0.0f : frame().width / static_cast<float>(items_.size());
        return Rect{frame().x + static_cast<float>(index) * itemWidth, frame().y, itemWidth, frame().height};
    }
    const float itemHeight = items_.empty() ? 0.0f : frame().height / static_cast<float>(items_.size());
    return Rect{frame().x, frame().y + static_cast<float>(index) * itemHeight, frame().width, itemHeight};
}

RadioGroupStyle RadioGroup::resolvedStyle(int index) const {
    const bool active = index == selectedIndex();
    RadioGroupStyle style = baseRadioGroupStyle(active, disabled(), index == hoveredIndex_, index == pressedIndex_);
    if (!styleOverride_) {
        return style;
    }

    if (styleOverride_->normal) {
        applyRadioGroupStateOverride(style, *styleOverride_->normal);
    }
    if (disabled() && styleOverride_->disabled) {
        applyRadioGroupStateOverride(style, *styleOverride_->disabled);
    } else if (index == pressedIndex_ && styleOverride_->pressed) {
        applyRadioGroupStateOverride(style, *styleOverride_->pressed);
    } else if (active && styleOverride_->selected) {
        applyRadioGroupStateOverride(style, *styleOverride_->selected);
    } else if (index == hoveredIndex_ && styleOverride_->hovered) {
        applyRadioGroupStateOverride(style, *styleOverride_->hovered);
    }
    if (focusVisible() && styleOverride_->focusVisible) {
        applyRadioGroupStateOverride(style, *styleOverride_->focusVisible);
    }

    return style;
}

void RadioGroup::assignSelectedIndex(int index) {
    if (items_.empty()) {
        const int previous = 0;
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

bool RadioGroup::hasInteractionState() const {
    return hoveredIndex_ >= 0 || pressedIndex_ >= 0;
}

void RadioGroup::resetInteractionState() {
    hoveredIndex_ = -1;
    pressedIndex_ = -1;
}

} // namespace oneui

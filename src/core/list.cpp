#include "oneui/controls/list.h"

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

void applyListStateOverride(ListStyle& style, const ListStateStyleOverride& override) {
    if (override.background) {
        style.background = *override.background;
    }
    if (override.border) {
        style.border = *override.border;
    }
    if (override.separator) {
        style.separator = *override.separator;
    }
    if (override.rowBackground) {
        style.rowBackground = *override.rowBackground;
    }
    if (override.titleColor) {
        style.titleColor = *override.titleColor;
    }
    if (override.detailColor) {
        style.detailColor = *override.detailColor;
    }
    if (override.selectedRowBackground) {
        style.selectedRowBackground = *override.selectedRowBackground;
    }
    if (override.selectedTitleColor) {
        style.selectedTitleColor = *override.selectedTitleColor;
    }
    if (override.selectedDetailColor) {
        style.selectedDetailColor = *override.selectedDetailColor;
    }
    if (override.borderWidth) {
        style.borderWidth = *override.borderWidth;
    }
    if (override.radius) {
        style.radius = *override.radius;
    }
    if (override.rowRadius) {
        style.rowRadius = *override.rowRadius;
    }
    if (override.rowInset) {
        style.rowInset = *override.rowInset;
    }
    if (override.textInset) {
        style.textInset = *override.textInset;
    }
    if (override.titleOffsetY) {
        style.titleOffsetY = *override.titleOffsetY;
    }
    if (override.detailOffsetY) {
        style.detailOffsetY = *override.detailOffsetY;
    }
    if (override.focusRing) {
        applyFocusRingOverride(style.focusRing, *override.focusRing);
    }
}

ListStyle baseListStyle(bool selected, bool disabled, bool hovered, bool pressed) {
    const auto& t = theme();
    ListStyle style;
    style.background = disabled ? t.surfaceMuted : t.surface;
    style.border = t.border;
    style.separator = t.border;
    style.rowBackground = Color{0, 0, 0, 0};
    style.titleColor = disabled ? t.textSubtle : t.text;
    style.detailColor = disabled ? t.textSubtle : t.textMuted;
    style.selectedRowBackground = t.primarySoft;
    style.selectedTitleColor = disabled ? t.textSubtle : t.primary;
    style.selectedDetailColor = disabled ? t.textSubtle : t.textMuted;
    style.borderWidth = 1.0f;
    style.radius = t.radiusMd;
    style.rowRadius = t.radiusSm;
    style.rowInset = Insets{3.0f, 4.0f};
    style.textInset = 12.0f;
    style.titleOffsetY = 7.0f;
    style.detailOffsetY = 27.0f;
    style.focusRing = FocusRingStyle{t.focusOutline, t.focusOutlineWidth, t.focusOutlineOffset, t.radiusLg, true};

    if (selected) {
        style.rowBackground = style.selectedRowBackground;
        style.titleColor = style.selectedTitleColor;
        style.detailColor = style.selectedDetailColor;
    } else if (!disabled && hovered) {
        style.rowBackground = t.surfaceMuted;
    }

    if (!disabled && pressed) {
        style.rowBackground = selected ? Color{191, 219, 254} : Color{232, 236, 242};
    }

    return style;
}

} // namespace

List::List() {
    setPreferredSize(Size{220.0f, 132.0f});
}

void List::setItems(std::vector<ListItem> items) {
    items_ = std::move(items);
    hoveredIndex_ = -1;
    pressedIndex_ = -1;
    assignSelectedIndex(selectedIndex());
    invalidate();
}

void List::setSelectedIndex(int index) {
    assignSelectedIndex(index);
}

int List::selectedIndex() const {
    return selectedBinding_.get(selectedIndex_);
}

void List::bindSelectedIndex(State<int>& state) {
    selectedBinding_ = Binding<int>(state, [this] {
        invalidate();
    });
    invalidate();
}

void List::setStyleOverride(ListStyleOverride style) {
    styleOverride_ = std::move(style);
    invalidate();
}

void List::clearStyleOverride() {
    styleOverride_.reset();
    invalidate();
}

void List::setOnChanged(std::function<void(int)> callback) {
    onChanged_ = std::move(callback);
}

void List::paint(Canvas& canvas) {
    const Rect rect = frame();
    const ListStyle containerStyle = resolvedContainerStyle();

    if (focusVisible() && !disabled() && containerStyle.focusRing.visible) {
        const float offset = containerStyle.focusRing.offset;
        canvas.strokeRect(
            Rect{rect.x - offset, rect.y - offset, rect.width + offset * 2.0f, rect.height + offset * 2.0f},
            containerStyle.focusRing.color,
            containerStyle.focusRing.radius,
            containerStyle.focusRing.width);
    }

    canvas.fillRect(rect, containerStyle.background, containerStyle.radius);
    canvas.strokeRect(rect, containerStyle.border, containerStyle.radius, containerStyle.borderWidth);

    for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
        const Rect row = itemRect(i);
        const ListStyle itemStyle = resolvedItemStyle(i);
        if (i > 0) {
            canvas.drawLine(Point{rect.x + itemStyle.textInset, row.y}, Point{rect.x + rect.width - itemStyle.textInset, row.y}, itemStyle.separator, 1.0f);
        }
        if (itemStyle.rowBackground.a > 0) {
            canvas.fillRect(row.inset(itemStyle.rowInset), itemStyle.rowBackground, itemStyle.rowRadius);
        }

        const auto& item = items_[static_cast<std::size_t>(i)];
        if (item.detail.empty()) {
            canvas.drawText(item.title, Rect{row.x + itemStyle.textInset, row.y, row.width - itemStyle.textInset * 2.0f, row.height}, itemStyle.titleColor, theme().fontMd, TextAlign::Left);
        } else {
            canvas.drawText(item.title, Rect{row.x + itemStyle.textInset, row.y + itemStyle.titleOffsetY, row.width - itemStyle.textInset * 2.0f, 18.0f}, itemStyle.titleColor, theme().fontMd, TextAlign::Left);
            canvas.drawText(item.detail, Rect{row.x + itemStyle.textInset, row.y + itemStyle.detailOffsetY, row.width - itemStyle.textInset * 2.0f, 16.0f}, itemStyle.detailColor, theme().fontSm, TextAlign::Left);
        }
    }
}

bool List::onMouseMove(const MouseEvent& event) {
    if (!interactive()) {
        return false;
    }

    const int next = hitItemIndex(event.position);
    if (next == hoveredIndex_) {
        return false;
    }

    hoveredIndex_ = next;
    invalidate();
    return true;
}

bool List::onMouseDown(const MouseEvent& event) {
    if (!interactive()) {
        return false;
    }

    pressedIndex_ = hitItemIndex(event.position);
    if (pressedIndex_ < 0) {
        return false;
    }

    invalidate();
    return true;
}

bool List::onMouseUp(const MouseEvent& event) {
    if (!interactive()) {
        return false;
    }

    const int wasPressed = pressedIndex_;
    pressedIndex_ = -1;
    if (wasPressed < 0) {
        return false;
    }

    if (hitItemIndex(event.position) == wasPressed) {
        assignSelectedIndex(wasPressed);
    }

    invalidate();
    return true;
}

bool List::onKeyDown(const KeyEvent& event) {
    if (!interactive() || items_.empty()) {
        return false;
    }

    const int selected = effectiveSelectedIndex();
    if (event.key == Key::Down) {
        assignSelectedIndex(std::min(static_cast<int>(items_.size()) - 1, selected + 1));
        return true;
    }

    if (event.key == Key::Up) {
        assignSelectedIndex(std::max(0, selected - 1));
        return true;
    }

    return false;
}

bool List::isFocusable() const {
    return interactive() && !items_.empty();
}

AccessibilityInfo List::accessibilityInfo() const {
    auto info = Widget::accessibilityInfo();
    if (info.role == AccessibilityRole::None) {
        info.role = AccessibilityRole::List;
    }
    if (!items_.empty()) {
        const auto& item = items_[static_cast<std::size_t>(effectiveSelectedIndex())];
        info.value = item.detail.empty() ? item.title : item.title + L" - " + item.detail;
        info.state.selected = true;
    }
    return info;
}

void List::assignSelectedIndex(int index) {
    if (items_.empty()) {
        const int previous = selectedIndex();
        selectedBinding_.set(0, selectedIndex_);
        if (previous != 0 && onChanged_) {
            onChanged_(0);
        }
        invalidate();
        return;
    }

    const int previous = effectiveSelectedIndex();
    const int next = std::clamp(index, 0, static_cast<int>(items_.size()) - 1);
    selectedBinding_.set(next, selectedIndex_);
    if (previous != next && onChanged_) {
        onChanged_(next);
    }
    invalidate();
}

int List::effectiveSelectedIndex() const {
    if (items_.empty()) {
        return 0;
    }
    return std::clamp(selectedIndex(), 0, static_cast<int>(items_.size()) - 1);
}

int List::hitItemIndex(Point point) const {
    if (items_.empty() || !contains(point)) {
        return -1;
    }

    const Rect rect = frame();
    const int index = static_cast<int>((point.y - rect.y) / rowHeight());
    if (index < 0 || index >= static_cast<int>(items_.size())) {
        return -1;
    }
    return index;
}

Rect List::itemRect(int index) const {
    const Rect rect = frame();
    return Rect{rect.x, rect.y + rowHeight() * static_cast<float>(index), rect.width, rowHeight()};
}

ListStyle List::resolvedContainerStyle() const {
    ListStyle style = baseListStyle(false, disabled(), false, false);
    if (!styleOverride_) {
        return style;
    }

    if (styleOverride_->normal) {
        applyListStateOverride(style, *styleOverride_->normal);
    }
    if (disabled() && styleOverride_->disabled) {
        applyListStateOverride(style, *styleOverride_->disabled);
    }
    if (focusVisible() && styleOverride_->focusVisible) {
        applyListStateOverride(style, *styleOverride_->focusVisible);
    }
    return style;
}

ListStyle List::resolvedItemStyle(int index) const {
    const bool active = index == effectiveSelectedIndex();
    const bool hovered = index == hoveredIndex_;
    const bool pressed = index == pressedIndex_;
    ListStyle style = baseListStyle(active, disabled(), hovered, pressed);
    if (!styleOverride_) {
        return style;
    }

    if (styleOverride_->normal) {
        applyListStateOverride(style, *styleOverride_->normal);
    }
    if (active) {
        style.rowBackground = style.selectedRowBackground;
        style.titleColor = style.selectedTitleColor;
        style.detailColor = style.selectedDetailColor;
    }
    if (disabled() && styleOverride_->disabled) {
        applyListStateOverride(style, *styleOverride_->disabled);
    } else if (pressed && styleOverride_->pressed) {
        applyListStateOverride(style, *styleOverride_->pressed);
    } else if (active && styleOverride_->selected) {
        applyListStateOverride(style, *styleOverride_->selected);
        style.rowBackground = style.selectedRowBackground;
        style.titleColor = style.selectedTitleColor;
        style.detailColor = style.selectedDetailColor;
    } else if (hovered && styleOverride_->hovered) {
        applyListStateOverride(style, *styleOverride_->hovered);
    }
    if (focusVisible() && styleOverride_->focusVisible) {
        applyListStateOverride(style, *styleOverride_->focusVisible);
    }
    return style;
}

float List::rowHeight() const {
    if (items_.empty()) {
        return 0.0f;
    }
    return frame().height / static_cast<float>(items_.size());
}

bool List::hasInteractionState() const {
    return hoveredIndex_ >= 0 || pressedIndex_ >= 0;
}

void List::resetInteractionState() {
    hoveredIndex_ = -1;
    pressedIndex_ = -1;
}

} // namespace oneui

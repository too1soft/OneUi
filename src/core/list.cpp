#include "oneui/controls/list.h"

#include "list_style_internal.h"

#include <algorithm>
#include <optional>
#include <utility>

namespace oneui {
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

void List::setSelectionRequired(bool required) {
    if (selectionRequired_ == required) {
        return;
    }
    selectionRequired_ = required;
    assignSelectedIndex(selectedIndex());
}

bool List::selectionRequired() const {
    return selectionRequired_;
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

    const ListStyle normalItemStyle = resolvedItemStyle(-1);
    for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
        const Rect row = itemRect(i);
        const bool hasRowState = i == effectiveSelectedIndex() || i == hoveredIndex_ || i == pressedIndex_;
        std::optional<ListStyle> stateItemStyle;
        if (hasRowState) {
            stateItemStyle = resolvedItemStyle(i);
        }
        const ListStyle& itemStyle = stateItemStyle ? *stateItemStyle : normalItemStyle;
        if (i > 0) {
            canvas.drawLine(Point{rect.x + itemStyle.textInset, row.y}, Point{rect.x + rect.width - itemStyle.textInset, row.y}, itemStyle.separator, 1.0f);
        }
        if (itemStyle.rowBackground.a > 0) {
            canvas.fillRect(row.inset(itemStyle.rowInset), itemStyle.rowBackground, itemStyle.rowRadius);
        }

        const auto& item = items_[static_cast<std::size_t>(i)];
        if (item.detail.empty()) {
            canvas.drawTextStyledEllipsized(item.title, Rect{row.x + itemStyle.textInset, row.y, row.width - itemStyle.textInset * 2.0f, row.height}, itemStyle.titleColor, itemStyle.titleFontSize, TextAlign::Left, itemStyle.titleFontWeight);
        } else {
            canvas.drawTextStyledEllipsized(item.title, Rect{row.x + itemStyle.textInset, row.y + itemStyle.titleOffsetY, row.width - itemStyle.textInset * 2.0f, 18.0f}, itemStyle.titleColor, itemStyle.titleFontSize, TextAlign::Left, itemStyle.titleFontWeight);
            canvas.drawTextStyledEllipsized(item.detail, Rect{row.x + itemStyle.textInset, row.y + itemStyle.detailOffsetY, row.width - itemStyle.textInset * 2.0f, 16.0f}, itemStyle.detailColor, itemStyle.detailFontSize, TextAlign::Left, itemStyle.detailFontWeight);
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
    const int selected = effectiveSelectedIndex();
    if (selected >= 0) {
        const auto& item = items_[static_cast<std::size_t>(selected)];
        info.value = item.detail.empty() ? item.title : item.title + L" - " + item.detail;
        info.state.selected = true;
    }
    return info;
}

void List::assignSelectedIndex(int index) {
    if (items_.empty()) {
        const int previous = selectedIndex();
        const int next = selectionRequired_ ? 0 : -1;
        selectedBinding_.set(next, selectedIndex_);
        if (previous != next && onChanged_) {
            onChanged_(next);
        }
        invalidate();
        return;
    }

    const int previous = effectiveSelectedIndex();
    const int minimum = selectionRequired_ ? 0 : -1;
    const int next = std::clamp(index, minimum, static_cast<int>(items_.size()) - 1);
    selectedBinding_.set(next, selectedIndex_);
    if (previous != next && onChanged_) {
        onChanged_(next);
    }
    invalidate();
}

int List::effectiveSelectedIndex() const {
    if (items_.empty()) {
        return selectionRequired_ ? 0 : -1;
    }
    const int minimum = selectionRequired_ ? 0 : -1;
    return std::clamp(selectedIndex(), minimum, static_cast<int>(items_.size()) - 1);
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
    ListStyle style = detail::baseListStyle(false, disabled(), false, false);
    if (!styleOverride_) {
        return style;
    }

    if (styleOverride_->normal) {
        detail::applyListStateOverride(style, *styleOverride_->normal);
    }
    if (disabled() && styleOverride_->disabled) {
        detail::applyListStateOverride(style, *styleOverride_->disabled);
    }
    if (focusVisible() && styleOverride_->focusVisible) {
        detail::applyListStateOverride(style, *styleOverride_->focusVisible);
    }
    return style;
}

ListStyle List::resolvedItemStyle(int index) const {
    const bool hasItem = index >= 0;
    const bool active = hasItem && index == effectiveSelectedIndex();
    const bool hovered = hasItem && index == hoveredIndex_;
    const bool pressed = hasItem && index == pressedIndex_;
    ListStyle style = detail::baseListStyle(active, disabled(), hovered, pressed);
    if (!styleOverride_) {
        return style;
    }

    if (styleOverride_->normal) {
        detail::applyListStateOverride(style, *styleOverride_->normal);
    }
    if (active) {
        style.rowBackground = style.selectedRowBackground;
        style.titleColor = style.selectedTitleColor;
        style.detailColor = style.selectedDetailColor;
    }
    if (disabled() && styleOverride_->disabled) {
        detail::applyListStateOverride(style, *styleOverride_->disabled);
    } else if (pressed && styleOverride_->pressed) {
        detail::applyListStateOverride(style, *styleOverride_->pressed);
    } else if (active && styleOverride_->selected) {
        detail::applyListStateOverride(style, *styleOverride_->selected);
        style.rowBackground = style.selectedRowBackground;
        style.titleColor = style.selectedTitleColor;
        style.detailColor = style.selectedDetailColor;
    } else if (hovered && styleOverride_->hovered) {
        detail::applyListStateOverride(style, *styleOverride_->hovered);
    }
    if (focusVisible() && styleOverride_->focusVisible) {
        detail::applyListStateOverride(style, *styleOverride_->focusVisible);
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

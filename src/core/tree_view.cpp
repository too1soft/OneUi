#include "oneui/controls/tree_view.h"

#include "oneui/style.h"
#include "reorder_internal.h"

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

void applyTreeViewStateOverride(TreeViewStyle& style, const TreeViewStateStyleOverride& override) {
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
    if (override.titleFontSize) {
        style.titleFontSize = std::max(1.0f, *override.titleFontSize);
    }
    if (override.detailFontSize) {
        style.detailFontSize = std::max(1.0f, *override.detailFontSize);
    }
    if (override.titleFontWeight) {
        style.titleFontWeight = std::clamp(*override.titleFontWeight, 100, 900);
    }
    if (override.detailFontWeight) {
        style.detailFontWeight = std::clamp(*override.detailFontWeight, 100, 900);
    }
    if (override.focusRing) {
        applyFocusRingOverride(style.focusRing, *override.focusRing);
    }
}

TreeViewStyle baseTreeViewStyle(bool selected, bool disabled, bool hovered, bool pressed) {
    const auto& t = theme();
    TreeViewStyle style;
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
    style.rowInset = Insets{2.0f, 3.0f};
    style.textInset = 8.0f;
    style.titleFontSize = t.fontMd;
    style.detailFontSize = t.fontSm;
    style.titleFontWeight = 400;
    style.detailFontWeight = 400;
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

TreeView::TreeView() {
    setPreferredSize(Size{240.0f, 260.0f});
    setAccessibleRole(AccessibilityRole::List);
    setAccessibleName(L"Tree");
}

void TreeView::setItems(std::vector<TreeItem> items) {
    items_ = std::move(items);
    rebuildHierarchy();
    hoveredIndex_ = -1;
    pressedIndex_ = -1;
    pressedToggle_ = false;
    resetReorderState();
    ensureSelectionVisible();
    updatePreferredHeight();
    invalidate();
}

void TreeView::setSelectedId(std::wstring id) {
    assignSelectedId(std::move(id));
}

const std::wstring& TreeView::selectedId() const {
    return selectedId_;
}

bool TreeView::isExpanded(const std::wstring& id) const {
    return expandedIds_.find(id) != expandedIds_.end();
}

void TreeView::setExpanded(std::wstring id, bool expanded) {
    const auto found = idToIndex_.find(id);
    if (found == idToIndex_.end() || !hasChildren(found->second)) {
        return;
    }

    const bool wasExpanded = isExpanded(id);
    if (wasExpanded == expanded) {
        return;
    }
    if (expanded) {
        expandedIds_.insert(id);
    } else {
        expandedIds_.erase(id);
    }
    ensureSelectionVisible();
    updatePreferredHeight();
    if (onExpansionChanged_) {
        onExpansionChanged_(id, expanded);
    }
    invalidate();
}

void TreeView::setStyleOverride(TreeViewStyleOverride style) {
    styleOverride_ = std::move(style);
    invalidate();
}

void TreeView::clearStyleOverride() {
    styleOverride_.reset();
    invalidate();
}

void TreeView::setOnSelectionChanged(std::function<void(const std::wstring&)> callback) {
    onSelectionChanged_ = std::move(callback);
}

void TreeView::setOnExpansionChanged(std::function<void(const std::wstring&, bool)> callback) {
    onExpansionChanged_ = std::move(callback);
}

void TreeView::setReorderEnabled(bool enabled) {
    if (reorderEnabled_ == enabled) {
        return;
    }
    reorderEnabled_ = enabled;
    resetReorderState();
    invalidate();
}

bool TreeView::reorderEnabled() const {
    return reorderEnabled_;
}

void TreeView::setOnReorderRequested(
    std::function<void(const std::wstring&, const std::wstring&)> callback) {
    onReorderRequested_ = std::move(callback);
}

std::size_t TreeView::visibleItemCount() const {
    return visibleItems().size();
}

float TreeView::contentHeight() const {
    return std::max(rowHeight(), rowHeight() * static_cast<float>(visibleItemCount()));
}

void TreeView::paint(Canvas& canvas) {
    const Rect rect = frame();
    const TreeViewStyle containerStyle = resolvedContainerStyle();
    const auto visible = visibleItems();

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

    for (int visibleIndex = 0; visibleIndex < static_cast<int>(visible.size()); ++visibleIndex) {
        const auto& visibleItem = visible[static_cast<std::size_t>(visibleIndex)];
        const auto& item = items_[visibleItem.index];
        const bool selected = item.id == selectedId_;
        const TreeViewStyle itemStyle = resolvedItemStyle(selected, visibleIndex == hoveredIndex_, visibleIndex == pressedIndex_);
        const Rect row = itemRect(visibleIndex);
        if (row.y >= rect.y + rect.height) {
            break;
        }
        if (itemStyle.rowBackground.a > 0) {
            canvas.fillRect(row.inset(itemStyle.rowInset), itemStyle.rowBackground, itemStyle.rowRadius);
        }

        const float indent = static_cast<float>(visibleItem.depth) * toggleWidth();
        const float toggleLeft = row.x + itemStyle.textInset + indent;
        const Rect toggleRect{toggleLeft, row.y, toggleWidth(), row.height};
        const bool expandable = hasChildren(visibleItem.index);
        if (expandable) {
            canvas.drawTextStyled(
                isExpanded(item.id) ? L"v" : L">",
                toggleRect,
                itemStyle.detailColor,
                itemStyle.detailFontSize,
                TextAlign::Center,
                itemStyle.detailFontWeight);
        }

        const float textLeft = toggleLeft + toggleWidth();
        const float detailWidth = item.detail.empty()
            ? 0.0f
            : canvas.measureTextWidth(
                item.detail,
                itemStyle.detailFontSize,
                itemStyle.detailFontWeight) + 8.0f;
        const float textWidth = std::max(0.0f, row.x + row.width - itemStyle.textInset - textLeft - detailWidth);
        canvas.drawTextStyledEllipsized(
            item.title,
            Rect{textLeft, row.y, textWidth, row.height},
            itemStyle.titleColor,
            itemStyle.titleFontSize,
            TextAlign::Left,
            itemStyle.titleFontWeight);
        if (!item.detail.empty()) {
            canvas.drawTextStyledEllipsized(
                item.detail,
                Rect{row.x + row.width - itemStyle.textInset - detailWidth, row.y, detailWidth, row.height},
                itemStyle.detailColor,
                itemStyle.detailFontSize,
                TextAlign::Right,
                itemStyle.detailFontWeight);
        }
    }
    if (reordering_ && reorderInsertionIndex_ >= 0) {
        const float rawY = rect.y + static_cast<float>(reorderInsertionIndex_) * rowHeight();
        const float indicatorY = std::clamp(rawY, rect.y + 1.0f, rect.y + rect.height - 1.0f);
        const float indicatorInset = std::max(4.0f, containerStyle.textInset);
        canvas.drawLine(
            Point{rect.x + indicatorInset, indicatorY},
            Point{rect.x + rect.width - indicatorInset, indicatorY},
            containerStyle.focusRing.color,
            std::max(2.0f, containerStyle.focusRing.width));
    }
}

bool TreeView::onMouseMove(const MouseEvent& event) {
    if (!interactive()) {
        return false;
    }
    const auto visible = visibleItems();
    if (reorderEnabled_ && reorderSourceIndex_ >= 0 && !pressedToggle_) {
        if (!reordering_ && detail::exceedsReorderDragThreshold(
                event.position.x - reorderStartPoint_.x,
                event.position.y - reorderStartPoint_.y)) {
            reordering_ = true;
        }
        if (reordering_) {
            updateReorderTarget(event.position, visible);
            hoveredIndex_ = hitItemIndex(event.position, visible);
            invalidate();
            return true;
        }
    }
    const int next = hitItemIndex(event.position, visible);
    if (next == hoveredIndex_) {
        return false;
    }
    hoveredIndex_ = next;
    invalidate();
    return true;
}

bool TreeView::onMouseDown(const MouseEvent& event) {
    if (!interactive() || event.button != MouseButton::Left) {
        return false;
    }
    resetReorderState();
    const auto visible = visibleItems();
    pressedIndex_ = hitItemIndex(event.position, visible);
    if (pressedIndex_ < 0) {
        return false;
    }
    pressedToggle_ = isToggleHit(event.position, visible[static_cast<std::size_t>(pressedIndex_)], pressedIndex_);
    if (reorderEnabled_ && !pressedToggle_) {
        reorderStartPoint_ = event.position;
        reorderSourceIndex_ = pressedIndex_;
        reorderTargetIndex_ = pressedIndex_;
    }
    invalidate();
    return true;
}

bool TreeView::onMouseUp(const MouseEvent& event) {
    if (!interactive() || event.button != MouseButton::Left) {
        return false;
    }
    const auto visible = visibleItems();
    if (reordering_) {
        updateReorderTarget(event.position, visible);
        const int sourceIndex = reorderSourceIndex_;
        const int targetIndex = reorderTargetIndex_;
        const bool valid = sourceIndex >= 0
            && sourceIndex < static_cast<int>(visible.size())
            && targetIndex >= 0
            && targetIndex < static_cast<int>(visible.size())
            && sourceIndex != targetIndex;
        const std::wstring sourceId = valid
            ? items_[visible[static_cast<std::size_t>(sourceIndex)].index].id
            : std::wstring{};
        const std::wstring targetId = valid
            ? items_[visible[static_cast<std::size_t>(targetIndex)].index].id
            : std::wstring{};
        const auto callback = onReorderRequested_;
        pressedIndex_ = -1;
        pressedToggle_ = false;
        resetReorderState();
        invalidate();
        if (valid && callback) {
            callback(sourceId, targetId);
        }
        return true;
    }
    const int wasPressed = pressedIndex_;
    const bool wasToggle = pressedToggle_;
    pressedIndex_ = -1;
    pressedToggle_ = false;
    resetReorderState();
    if (wasPressed < 0 || hitItemIndex(event.position, visible) != wasPressed) {
        invalidate();
        return wasPressed >= 0;
    }

    const auto& visibleItem = visible[static_cast<std::size_t>(wasPressed)];
    if (wasToggle) {
        toggleItem(visibleItem.index);
    } else {
        assignSelectedId(items_[visibleItem.index].id);
    }
    invalidate();
    return true;
}

bool TreeView::onKeyDown(const KeyEvent& event) {
    if (!interactive()) {
        return false;
    }
    const auto visible = visibleItems();
    if (visible.empty()) {
        return false;
    }

    const int selected = visibleIndexForId(selectedId_, visible);
    const int effectiveSelected = selected >= 0 ? selected : 0;
    const auto& current = visible[static_cast<std::size_t>(effectiveSelected)];

    if (reorderEnabled_ && event.alt && !event.control
        && (event.key == Key::Up || event.key == Key::Down)) {
        const int target = event.key == Key::Up
            ? std::max(0, effectiveSelected - 1)
            : std::min(static_cast<int>(visible.size()) - 1, effectiveSelected + 1);
        const auto callback = onReorderRequested_;
        if (target != effectiveSelected && callback) {
            const std::wstring sourceId = items_[current.index].id;
            const std::wstring targetId = items_[visible[static_cast<std::size_t>(target)].index].id;
            callback(sourceId, targetId);
        }
        return true;
    }

    if (event.key == Key::Down) {
        const int next = std::min(static_cast<int>(visible.size()) - 1, effectiveSelected + 1);
        assignSelectedId(items_[visible[static_cast<std::size_t>(next)].index].id);
        return true;
    }
    if (event.key == Key::Up) {
        const int previous = std::max(0, effectiveSelected - 1);
        assignSelectedId(items_[visible[static_cast<std::size_t>(previous)].index].id);
        return true;
    }
    if (event.key == Key::Right) {
        if (hasChildren(current.index) && !isExpanded(items_[current.index].id)) {
            toggleItem(current.index);
        } else if (hasChildren(current.index)) {
            assignSelectedId(items_[children_[current.index].front()].id);
        }
        return true;
    }
    if (event.key == Key::Left) {
        if (hasChildren(current.index) && isExpanded(items_[current.index].id)) {
            toggleItem(current.index);
        } else if (const auto parent = parentIndex(current.index)) {
            assignSelectedId(items_[*parent].id);
        }
        return true;
    }
    if ((event.key == Key::Space || event.key == Key::Enter) && hasChildren(current.index)) {
        toggleItem(current.index);
        return true;
    }
    return false;
}

bool TreeView::isFocusable() const {
    return interactive() && !visibleItems().empty();
}

AccessibilityInfo TreeView::accessibilityInfo() const {
    auto info = Widget::accessibilityInfo();
    if (info.role == AccessibilityRole::None) {
        info.role = AccessibilityRole::List;
    }
    const auto found = idToIndex_.find(selectedId_);
    if (found != idToIndex_.end()) {
        const auto& item = items_[found->second];
        info.value = item.detail.empty() ? item.title : item.title + L" - " + item.detail;
        info.state.selected = true;
        info.state.expanded = hasChildren(found->second) && isExpanded(item.id);
    }
    return info;
}

void TreeView::rebuildHierarchy() {
    std::vector<TreeItem> uniqueItems;
    uniqueItems.reserve(items_.size());
    idToIndex_.clear();
    for (auto& item : items_) {
        if (item.id.empty() || idToIndex_.find(item.id) != idToIndex_.end()) {
            continue;
        }
        const std::size_t index = uniqueItems.size();
        idToIndex_.emplace(item.id, index);
        uniqueItems.push_back(std::move(item));
    }
    items_ = std::move(uniqueItems);
    children_.assign(items_.size(), {});
    roots_.clear();
    expandedIds_.clear();
    for (const auto& item : items_) {
        if (item.expanded) {
            expandedIds_.insert(item.id);
        }
    }

    for (std::size_t index = 0; index < items_.size(); ++index) {
        const auto parent = idToIndex_.find(items_[index].parentId);
        bool validParent = parent != idToIndex_.end() && parent->second != index;
        if (validParent) {
            std::unordered_set<std::size_t> visited{index};
            std::size_t cursor = parent->second;
            while (true) {
                if (!visited.insert(cursor).second) {
                    validParent = false;
                    break;
                }
                const auto ancestor = idToIndex_.find(items_[cursor].parentId);
                if (ancestor == idToIndex_.end() || ancestor->second == cursor) {
                    break;
                }
                cursor = ancestor->second;
            }
        }
        if (validParent) {
            children_[parent->second].push_back(index);
        } else {
            roots_.push_back(index);
        }
    }
}

std::vector<TreeView::VisibleItem> TreeView::visibleItems() const {
    std::vector<VisibleItem> result;
    result.reserve(items_.size());
    for (const std::size_t root : roots_) {
        appendVisibleItems(result, root, 0);
    }
    return result;
}

void TreeView::appendVisibleItems(std::vector<VisibleItem>& output, std::size_t index, int depth) const {
    output.push_back(VisibleItem{index, depth});
    if (!isExpanded(items_[index].id)) {
        return;
    }
    for (const std::size_t child : children_[index]) {
        appendVisibleItems(output, child, depth + 1);
    }
}

void TreeView::assignSelectedId(std::wstring id) {
    const auto visible = visibleItems();
    if (visible.empty()) {
        id.clear();
    } else if (visibleIndexForId(id, visible) < 0) {
        id = items_[visible.front().index].id;
    }
    if (selectedId_ == id) {
        return;
    }
    selectedId_ = std::move(id);
    if (onSelectionChanged_) {
        onSelectionChanged_(selectedId_);
    }
    invalidate();
}

void TreeView::ensureSelectionVisible() {
    const auto visible = visibleItems();
    if (visible.empty()) {
        if (!selectedId_.empty()) {
            assignSelectedId({});
        }
        return;
    }
    if (visibleIndexForId(selectedId_, visible) < 0) {
        const std::wstring next = items_[visible.front().index].id;
        if (selectedId_.empty()) {
            selectedId_ = next;
        } else {
            assignSelectedId(next);
        }
    }
}

int TreeView::visibleIndexForId(const std::wstring& id, const std::vector<VisibleItem>& visible) const {
    for (int index = 0; index < static_cast<int>(visible.size()); ++index) {
        if (items_[visible[static_cast<std::size_t>(index)].index].id == id) {
            return index;
        }
    }
    return -1;
}

int TreeView::hitItemIndex(Point point, const std::vector<VisibleItem>& visible) const {
    if (!contains(point) || visible.empty()) {
        return -1;
    }
    const int index = static_cast<int>((point.y - frame().y) / rowHeight());
    if (index < 0 || index >= static_cast<int>(visible.size())) {
        return -1;
    }
    return index;
}

Rect TreeView::itemRect(int visibleIndex) const {
    const Rect rect = frame();
    return Rect{rect.x, rect.y + rowHeight() * static_cast<float>(visibleIndex), rect.width, rowHeight()};
}

bool TreeView::isToggleHit(Point point, const VisibleItem& item, int visibleIndex) const {
    if (!hasChildren(item.index)) {
        return false;
    }
    const TreeViewStyle style = resolvedItemStyle(items_[item.index].id == selectedId_, false, false);
    const Rect row = itemRect(visibleIndex);
    const float left = row.x + style.textInset + static_cast<float>(item.depth) * toggleWidth();
    return Rect{left, row.y, toggleWidth(), row.height}.contains(point);
}

void TreeView::resetReorderState() {
    reordering_ = false;
    reorderStartPoint_ = {};
    reorderSourceIndex_ = -1;
    reorderTargetIndex_ = -1;
    reorderInsertionIndex_ = -1;
}

void TreeView::updateReorderTarget(Point point, const std::vector<VisibleItem>& visible) {
    reorderInsertionIndex_ = detail::reorderInsertionIndex(
        point.y,
        frame().y,
        0.0f,
        rowHeight(),
        static_cast<int>(visible.size()));
    reorderTargetIndex_ = detail::reorderTargetIndex(
        reorderSourceIndex_,
        reorderInsertionIndex_,
        static_cast<int>(visible.size()));
}

bool TreeView::hasChildren(std::size_t index) const {
    return index < children_.size() && !children_[index].empty();
}

void TreeView::toggleItem(std::size_t index) {
    if (!hasChildren(index)) {
        return;
    }
    setExpanded(items_[index].id, !isExpanded(items_[index].id));
}

std::optional<std::size_t> TreeView::parentIndex(std::size_t index) const {
    if (index >= items_.size()) {
        return std::nullopt;
    }
    const auto parent = idToIndex_.find(items_[index].parentId);
    if (parent == idToIndex_.end() || parent->second == index) {
        return std::nullopt;
    }
    const auto& siblings = children_[parent->second];
    return std::find(siblings.begin(), siblings.end(), index) != siblings.end()
        ? std::optional<std::size_t>(parent->second)
        : std::nullopt;
}

TreeViewStyle TreeView::resolvedContainerStyle() const {
    TreeViewStyle style = baseTreeViewStyle(false, disabled(), false, false);
    if (!styleOverride_) {
        return style;
    }
    if (styleOverride_->normal) {
        applyTreeViewStateOverride(style, *styleOverride_->normal);
    }
    if (disabled() && styleOverride_->disabled) {
        applyTreeViewStateOverride(style, *styleOverride_->disabled);
    }
    if (focusVisible() && styleOverride_->focusVisible) {
        applyTreeViewStateOverride(style, *styleOverride_->focusVisible);
    }
    return style;
}

TreeViewStyle TreeView::resolvedItemStyle(bool selected, bool hovered, bool pressed) const {
    TreeViewStyle style = baseTreeViewStyle(selected, disabled(), hovered, pressed);
    if (!styleOverride_) {
        return style;
    }
    if (styleOverride_->normal) {
        applyTreeViewStateOverride(style, *styleOverride_->normal);
    }
    if (selected) {
        style.rowBackground = style.selectedRowBackground;
        style.titleColor = style.selectedTitleColor;
        style.detailColor = style.selectedDetailColor;
    }
    if (disabled() && styleOverride_->disabled) {
        applyTreeViewStateOverride(style, *styleOverride_->disabled);
    } else if (pressed && styleOverride_->pressed) {
        applyTreeViewStateOverride(style, *styleOverride_->pressed);
    } else if (selected && styleOverride_->selected) {
        applyTreeViewStateOverride(style, *styleOverride_->selected);
        style.rowBackground = style.selectedRowBackground;
        style.titleColor = style.selectedTitleColor;
        style.detailColor = style.selectedDetailColor;
    } else if (hovered && styleOverride_->hovered) {
        applyTreeViewStateOverride(style, *styleOverride_->hovered);
    }
    if (focusVisible() && styleOverride_->focusVisible) {
        applyTreeViewStateOverride(style, *styleOverride_->focusVisible);
    }
    return style;
}

float TreeView::rowHeight() const {
    return 32.0f;
}

float TreeView::toggleWidth() const {
    return 18.0f;
}

void TreeView::updatePreferredHeight() {
    const Size current = preferredSize();
    setPreferredSize(Size{current.width, contentHeight()});
}

bool TreeView::hasInteractionState() const {
    return hoveredIndex_ >= 0 || pressedIndex_ >= 0;
}

void TreeView::resetInteractionState() {
    hoveredIndex_ = -1;
    pressedIndex_ = -1;
    pressedToggle_ = false;
    resetReorderState();
}

} // namespace oneui

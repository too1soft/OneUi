#include "oneui/controls/tree_view.h"

#include "oneui/style.h"
#include "reorder_internal.h"

#include <algorithm>
#include <cstdint>
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

bool hasClass(const std::string& classes, const char* token) {
    const std::string needle(token);
    std::size_t at = 0;
    while (at < classes.size()) {
        const auto end = classes.find(' ', at);
        const auto length = (end == std::string::npos ? classes.size() : end) - at;
        if (classes.compare(at, length, needle) == 0) return true;
        if (end == std::string::npos) break;
        at = end + 1;
    }
    return false;
}

Color alpha(Color color, std::uint8_t a) {
    color.a = a;
    return color;
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
    clearExternalDropTarget();
    resetReorderState();
    clampScroll();
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
    if (found == idToIndex_.end() || !expandable(found->second)) {
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
    visibleDirty_ = true;
    clampScroll();
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

void TreeView::setInternalScrolling(bool enabled) {
    if (internalScrolling_ == enabled) return;
    internalScrolling_ = enabled;
    scrollOffset_ = 0.0f;
    updatePreferredHeight();
    invalidate();
}

bool TreeView::internalScrolling() const { return internalScrolling_; }

void TreeView::setScrollOffset(float offset) {
    const float next = std::clamp(offset, 0.0f, maxScrollOffset());
    if (next == scrollOffset_) return;
    scrollOffset_ = next;
    hoveredIndex_ = -1;
    invalidate();
}

float TreeView::scrollOffset() const { return scrollOffset_; }

float TreeView::maxScrollOffset() const {
    if (!internalScrolling_) return 0.0f;
    return std::max(0.0f, rowHeight() * static_cast<float>(visibleItems().size()) - frame().height);
}

void TreeView::scrollToId(const std::wstring& id) {
    if (!internalScrolling_) return;
    const int index = visibleIndexForId(id, visibleItems());
    if (index < 0) return;
    const float top = rowHeight() * static_cast<float>(index);
    if (top < scrollOffset_) setScrollOffset(top);
    else if (top + rowHeight() > scrollOffset_ + frame().height) setScrollOffset(top + rowHeight() - frame().height);
}

void TreeView::setRowHeight(float height) {
    height = std::clamp(height, 16.0f, 96.0f);
    if (height == rowHeight_) return;
    rowHeight_ = height;
    clampScroll();
    updatePreferredHeight();
    invalidate();
}

void TreeView::setRetainExpansion(bool retain) {
    retainExpansion_ = retain;
}

void TreeView::setLoadingText(std::wstring text) {
    loadingText_ = std::move(text);
    invalidate();
}

void TreeView::setOnActivated(std::function<void(const std::wstring&)> callback) {
    onActivated_ = std::move(callback);
}

void TreeView::setOnContextMenu(std::function<void(const std::wstring&, Point)> callback) {
    onContextMenu_ = std::move(callback);
}

bool TreeView::onMouseWheel(const MouseWheelEvent& event) {
    if (!interactive() || !contains(event.position) || maxScrollOffset() <= 0.0f) return false;
    setScrollOffset(scrollOffset_ - event.deltaY * rowHeight() * 3.0f);
    return true;
}

void TreeView::clampScroll() {
    scrollOffset_ = std::clamp(scrollOffset_, 0.0f, maxScrollOffset());
}

void TreeView::updateExternalDropTarget(Point point) {
    const auto& visible = visibleItems();
    const int next = hitItemIndex(point, visible);
    const std::wstring nextId = next >= 0
        ? items_[visible[static_cast<std::size_t>(next)].index].id
        : std::wstring{};
    if (next == externalDropTargetIndex_ && nextId == externalDropTargetId_) {
        return;
    }
    externalDropTargetIndex_ = next;
    externalDropTargetId_ = nextId;
    invalidate();
}

void TreeView::clearExternalDropTarget() {
    if (externalDropTargetIndex_ < 0 && externalDropTargetId_.empty()) {
        return;
    }
    externalDropTargetIndex_ = -1;
    externalDropTargetId_.clear();
    invalidate();
}

const std::wstring& TreeView::externalDropTargetId() const {
    return externalDropTargetId_;
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
    const auto& visible = visibleItems();

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

    canvas.save();
    canvas.clipRect(rect);
    // Only rows intersecting the frame are painted, so large trees cost the
    // same per frame as small ones.
    const int first = std::max(0, static_cast<int>(scrollOffset_ / rowHeight()));
    for (int visibleIndex = first; visibleIndex < static_cast<int>(visible.size()); ++visibleIndex) {
        if (itemRect(visibleIndex).y >= rect.y + rect.height) {
            break;
        }
        paintItem(canvas, visible[static_cast<std::size_t>(visibleIndex)], visibleIndex);
    }
    if (reordering_ && reorderInsertionIndex_ >= 0) {
        const float rawY = rect.y + static_cast<float>(reorderInsertionIndex_) * rowHeight() - scrollOffset_;
        const float indicatorY = std::clamp(rawY, rect.y + 1.0f, rect.y + rect.height - 1.0f);
        const float indicatorInset = std::max(4.0f, containerStyle.textInset);
        canvas.drawLine(
            Point{rect.x + indicatorInset, indicatorY},
            Point{rect.x + rect.width - indicatorInset, indicatorY},
            containerStyle.focusRing.color,
            std::max(2.0f, containerStyle.focusRing.width));
    }
    canvas.restore();
}

void TreeView::paintItem(Canvas& canvas, const VisibleItem& visibleItem, int visibleIndex) {
    const auto& item = items_[visibleItem.index];
    const Rect row = itemRect(visibleIndex);
    if (visibleItem.placeholder) {
        const TreeViewStyle style = resolvedItemStyle(false, false, false);
        const float left = row.x + style.textInset + static_cast<float>(visibleItem.depth + 1) * toggleWidth();
        canvas.drawTextStyledEllipsized(loadingText_, Rect{left, row.y, std::max(0.0f, row.x + row.width - left), row.height},
                                        style.detailColor, style.detailFontSize, TextAlign::Left, style.detailFontWeight);
        return;
    }
    const bool selected = item.id == selectedId_;
    const bool hovered = visibleIndex == hoveredIndex_
        || visibleIndex == externalDropTargetIndex_;
    TreeViewStyle itemStyle = resolvedItemStyle(selected, hovered, visibleIndex == pressedIndex_);
    const auto& t = theme();
    if (!selected && !item.classes.empty()) {
        if (hasClass(item.classes, "muted")) itemStyle.titleColor = t.textSubtle;
        if (hasClass(item.classes, "error")) itemStyle.titleColor = t.formField.errorColor;
        if (hasClass(item.classes, "warning")) itemStyle.titleColor = t.warning;
        if (hasClass(item.classes, "accent")) itemStyle.titleColor = t.primary;
    }
    if (hasClass(item.classes, "strong")) itemStyle.titleFontWeight = 600;
    if (itemStyle.rowBackground.a > 0) {
        canvas.fillRect(row.inset(itemStyle.rowInset), itemStyle.rowBackground, itemStyle.rowRadius);
    }

    const float indent = static_cast<float>(visibleItem.depth) * toggleWidth();
    const float toggleLeft = row.x + itemStyle.textInset + indent;
    const Rect toggleRect{toggleLeft, row.y, toggleWidth(), row.height};
    if (expandable(visibleItem.index)) {
        canvas.drawTextStyled(
            isExpanded(item.id) ? L"v" : L">",
            toggleRect,
            itemStyle.detailColor,
            itemStyle.detailFontSize,
            TextAlign::Center,
            itemStyle.detailFontWeight);
    }

    float textLeft = toggleLeft + toggleWidth();
    if (item.icon) {
        const float size = std::min(16.0f, row.height - 8.0f);
        paintIcon(canvas, *item.icon, Rect{textLeft, row.y + (row.height - size) * 0.5f, size, size}, itemStyle.detailColor,
                  t.primary);
        textLeft += size + 6.0f;
    }
    float right = row.x + row.width - itemStyle.textInset;
    if (!item.detail.empty()) {
        const float detailWidth = canvas.measureTextWidth(item.detail, itemStyle.detailFontSize, itemStyle.detailFontWeight) + 8.0f;
        canvas.drawTextStyledEllipsized(
            item.detail,
            Rect{right - detailWidth, row.y, detailWidth, row.height},
            itemStyle.detailColor,
            itemStyle.detailFontSize,
            TextAlign::Right,
            itemStyle.detailFontWeight);
        right -= detailWidth;
    }
    if (!item.badge.empty()) {
        const float badgeWidth = canvas.measureTextWidth(item.badge, itemStyle.detailFontSize, 500) + 12.0f;
        const float badgeHeight = std::min(18.0f, row.height - 8.0f);
        const Rect badge{right - badgeWidth, row.y + (row.height - badgeHeight) * 0.5f, badgeWidth, badgeHeight};
        canvas.fillRect(badge, t.surfaceMuted, badgeHeight * 0.5f);
        canvas.drawTextStyled(item.badge, badge, itemStyle.detailColor, itemStyle.detailFontSize, TextAlign::Center, 500);
        right -= badgeWidth + 6.0f;
    }
    const float textWidth = std::max(0.0f, right - textLeft);
    if (item.highlightLength > 0 && item.highlightStart < item.title.size()) {
        const auto start = item.highlightStart;
        const auto length = std::min(item.highlightLength, item.title.size() - start);
        const float before = canvas.measureTextWidth(item.title.substr(0, start), itemStyle.titleFontSize, itemStyle.titleFontWeight);
        const float width = canvas.measureTextWidth(item.title.substr(start, length), itemStyle.titleFontSize, itemStyle.titleFontWeight);
        const float x = textLeft + before;
        if (x < textLeft + textWidth) {
            const float height = itemStyle.titleFontSize + 6.0f;
            canvas.fillRect(Rect{x - 1.0f, row.y + (row.height - height) * 0.5f, std::min(width + 2.0f, textLeft + textWidth - x + 1.0f), height},
                            alpha(t.warning, 70), 3.0f);
        }
    }
    canvas.drawTextStyledEllipsized(
        item.title,
        Rect{textLeft, row.y, textWidth, row.height},
        itemStyle.titleColor,
        itemStyle.titleFontSize,
        TextAlign::Left,
        itemStyle.titleFontWeight);
}

bool TreeView::onMouseMove(const MouseEvent& event) {
    if (!interactive()) {
        return false;
    }
    const auto& visible = visibleItems();
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
    if (!interactive()) {
        return false;
    }
    if (event.button == MouseButton::Right) {
        const auto& visible = visibleItems();
        const int index = hitItemIndex(event.position, visible);
        if (index < 0 || visible[static_cast<std::size_t>(index)].placeholder || !onContextMenu_) {
            return false;
        }
        assignSelectedId(items_[visible[static_cast<std::size_t>(index)].index].id);
        onContextMenu_(selectedId_, event.position);
        return true;
    }
    if (event.button != MouseButton::Left) {
        return false;
    }
    resetReorderState();
    const auto& visible = visibleItems();
    pressedIndex_ = hitItemIndex(event.position, visible);
    if (pressedIndex_ < 0) {
        return false;
    }
    if (visible[static_cast<std::size_t>(pressedIndex_)].placeholder) {
        pressedIndex_ = -1;
        return true;
    }
    pressedToggle_ = isToggleHit(event.position, visible[static_cast<std::size_t>(pressedIndex_)], pressedIndex_);
    if (event.clickCount >= 2 && !pressedToggle_ && onActivated_) {
        const std::wstring id = items_[visible[static_cast<std::size_t>(pressedIndex_)].index].id;
        pressedIndex_ = -1;
        assignSelectedId(id);
        onActivated_(id);
        invalidate();
        return true;
    }
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
    const auto& visible = visibleItems();
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
    const auto& visible = visibleItems();
    if (visible.empty()) {
        return false;
    }

    const int selected = visibleIndexForId(selectedId_, visible);
    const int effectiveSelected = selected >= 0 ? selected : 0;
    // Copied: toggling rebuilds the visible row cache.
    const auto current = visible[static_cast<std::size_t>(effectiveSelected)];
    constexpr unsigned appsKey = 0x5d, f10Key = 0x79;
    if (event.virtualKey == appsKey || (event.virtualKey == f10Key && event.shift)) {
        if (!onContextMenu_ || selectedId_.empty()) return false;
        const Rect row = itemRect(effectiveSelected);
        onContextMenu_(selectedId_, Point{row.x + 24.0f, row.y + row.height});
        return true;
    }

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

    const int page = std::max(1, static_cast<int>(frame().height / rowHeight()) - 1);
    const int last = static_cast<int>(visible.size()) - 1;
    int step = 0;
    if (event.key == Key::Down) step = 1;
    else if (event.key == Key::Up) step = -1;
    else if (event.key == Key::PageDown) step = page;
    else if (event.key == Key::PageUp) step = -page;
    else if (event.key == Key::Home) step = -last - 1;
    else if (event.key == Key::End) step = last + 1;
    if (step != 0) {
        const int next = stepSelectable(effectiveSelected, step, visible);
        assignSelectedId(items_[visible[static_cast<std::size_t>(next)].index].id);
        return true;
    }
    if (event.key == Key::Enter && onActivated_ && !selectedId_.empty()) {
        onActivated_(selectedId_);
        return true;
    }
    if (event.key == Key::Right) {
        if (expandable(current.index) && !isExpanded(items_[current.index].id)) {
            toggleItem(current.index);
        } else if (hasChildren(current.index)) {
            assignSelectedId(items_[children_[current.index].front()].id);
        }
        return true;
    }
    if (event.key == Key::Left) {
        if (expandable(current.index) && isExpanded(items_[current.index].id)) {
            toggleItem(current.index);
        } else if (const auto parent = parentIndex(current.index)) {
            assignSelectedId(items_[*parent].id);
        }
        return true;
    }
    if ((event.key == Key::Space || event.key == Key::Enter) && expandable(current.index)) {
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
        info.state.expanded = expandable(found->second) && isExpanded(item.id);
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
    visibleDirty_ = true;
    children_.assign(items_.size(), {});
    roots_.clear();
    std::unordered_set<std::wstring> expanded;
    std::unordered_map<std::wstring, bool> supplied;
    supplied.reserve(items_.size());
    for (const auto& item : items_) {
        bool open = item.expanded;
        if (retainExpansion_) {
            const auto previous = suppliedExpanded_.find(item.id);
            if (previous != suppliedExpanded_.end() && previous->second == item.expanded) {
                open = expandedIds_.count(item.id) != 0;
            }
        }
        if (open) {
            expanded.insert(item.id);
        }
        supplied.emplace(item.id, item.expanded);
    }
    expandedIds_ = std::move(expanded);
    suppliedExpanded_ = std::move(supplied);

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

const std::vector<TreeView::VisibleItem>& TreeView::visibleItems() const {
    if (visibleDirty_) {
        visibleCache_.clear();
        visibleCache_.reserve(items_.size());
        for (const std::size_t root : roots_) {
            appendVisibleItems(visibleCache_, root, 0);
        }
        visibleDirty_ = false;
    }
    return visibleCache_;
}

bool TreeView::expandable(std::size_t index) const {
    return hasChildren(index) || (index < items_.size() && items_[index].hasChildren);
}

int TreeView::stepSelectable(int from, int step, const std::vector<VisibleItem>& visible) const {
    const int last = static_cast<int>(visible.size()) - 1;
    const int direction = step > 0 ? 1 : -1;
    int next = std::clamp(from + step, 0, last);
    while (next >= 0 && next <= last && visible[static_cast<std::size_t>(next)].placeholder) next += direction;
    if (next < 0 || next > last) {
        // Ran past the end: back off to the nearest real row.
        next = std::clamp(next, 0, last);
        while (next >= 0 && next <= last && visible[static_cast<std::size_t>(next)].placeholder) next -= direction;
    }
    return std::clamp(next, 0, last);
}

void TreeView::appendVisibleItems(std::vector<VisibleItem>& output, std::size_t index, int depth) const {
    output.push_back(VisibleItem{index, depth});
    if (!isExpanded(items_[index].id)) {
        return;
    }
    if (children_[index].empty() && items_[index].loading) {
        output.push_back(VisibleItem{index, depth, true});
        return;
    }
    for (const std::size_t child : children_[index]) {
        appendVisibleItems(output, child, depth + 1);
    }
}

void TreeView::assignSelectedId(std::wstring id) {
    const auto& visible = visibleItems();
    if (visible.empty()) {
        id.clear();
    } else if (visibleIndexForId(id, visible) < 0) {
        id = items_[visible.front().index].id;
    }
    if (selectedId_ == id) {
        return;
    }
    selectedId_ = std::move(id);
    scrollToId(selectedId_);
    if (onSelectionChanged_) {
        onSelectionChanged_(selectedId_);
    }
    invalidate();
}

void TreeView::ensureSelectionVisible() {
    const auto& visible = visibleItems();
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
        const auto& row = visible[static_cast<std::size_t>(index)];
        if (!row.placeholder && items_[row.index].id == id) {
            return index;
        }
    }
    return -1;
}

int TreeView::hitItemIndex(Point point, const std::vector<VisibleItem>& visible) const {
    if (!contains(point) || visible.empty()) {
        return -1;
    }
    const int index = static_cast<int>((point.y - frame().y + scrollOffset_) / rowHeight());
    if (index < 0 || index >= static_cast<int>(visible.size())) {
        return -1;
    }
    return index;
}

Rect TreeView::itemRect(int visibleIndex) const {
    const Rect rect = frame();
    return Rect{rect.x, rect.y + rowHeight() * static_cast<float>(visibleIndex) - scrollOffset_, rect.width, rowHeight()};
}

bool TreeView::isToggleHit(Point point, const VisibleItem& item, int visibleIndex) const {
    if (item.placeholder || !expandable(item.index)) {
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
        scrollOffset_,
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
    if (!expandable(index)) {
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
    return rowHeight_;
}

float TreeView::toggleWidth() const {
    return 18.0f;
}

void TreeView::updatePreferredHeight() {
    if (internalScrolling_) {
        return;
    }
    const Size current = preferredSize();
    setPreferredSize(Size{current.width, contentHeight()});
}

bool TreeView::hasInteractionState() const {
    return hoveredIndex_ >= 0 || pressedIndex_ >= 0 || externalDropTargetIndex_ >= 0;
}

void TreeView::resetInteractionState() {
    hoveredIndex_ = -1;
    pressedIndex_ = -1;
    pressedToggle_ = false;
    resetReorderState();
    clearExternalDropTarget();
}

} // namespace oneui

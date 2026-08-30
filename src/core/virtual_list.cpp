#include "oneui/controls/virtual_list.h"

#include "internal/scroll_trace.h"
#include "list_style_internal.h"
#include "reorder_internal.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <optional>
#include <unordered_set>
#include <utility>

namespace oneui {
namespace {

double currentTimeMs() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration<double, std::milli>(now).count();
}

} // namespace

VirtualList::VirtualList() {
    setPreferredSize(Size{220.0f, 264.0f});
}

void VirtualList::setItems(std::vector<ListItem> items) {
    const int dragSource = reorderSourceIndex_;
    const bool notifyCancellation = externalDragging_ && itemDragEnabled_ && onItemDrag_
        && dragSource >= 0 && dragSource < static_cast<int>(itemDragIds_.size());
    const ItemDragEvent cancellation{
        notifyCancellation ? itemDragIds_[static_cast<std::size_t>(dragSource)] : std::wstring{},
        ItemDragPhase::Cancelled,
        reorderCurrentPoint_};
    const auto dragCallback = onItemDrag_;
    const auto previousIndices = selection_.selectedIndices();
    const int previousSelectedIndex = selectedIndex();
    const bool wasUninitialized = selection_.itemCount() == 0
        && previousIndices.empty()
        && selection_.activeIndex() < 0;
    items_ = std::move(items);
    itemDragIds_.clear();
    selection_.setItemCount(static_cast<int>(items_.size()));
    if (wasUninitialized && !items_.empty()) {
        selection_.selectOnly(0);
    }
    hoveredIndex_ = -1;
    pressedIndex_ = -1;
    pressedClickCount_ = 1;
    resetReorderState();
    scrollOffset_ = std::clamp(scrollOffset_, 0.0f, maxScrollOffset());
    resetScrollMotion(scrollOffset_);
    if (selection_.activeIndex() >= 0) {
        ensureSelectionVisible();
    }
    notifySelectionChanged(previousIndices, previousSelectedIndex);
    invalidate();
    if (notifyCancellation) {
        dragCallback(cancellation);
    }
}

bool VirtualList::updateItem(std::size_t index, ListItem item) {
    if (index >= items_.size()) {
        return false;
    }
    if (items_[index].title == item.title && items_[index].detail == item.detail) {
        return true;
    }
    items_[index] = std::move(item);
    invalidate();
    return true;
}

void VirtualList::setSelectedIndex(int index) {
    assignSelectedIndex(index);
}

int VirtualList::selectedIndex() const {
    const int active = selection_.activeIndex();
    if (selection_.contains(active)) {
        return active;
    }
    return selection_.selectedIndices().empty() ? -1 : selection_.selectedIndices().back();
}

const std::vector<ListItem>& VirtualList::items() const {
    return items_;
}

Rect VirtualList::itemFrame(int index) const {
    return itemRect(index);
}

void VirtualList::setSelectionMode(SelectionMode mode) {
    const auto previousIndices = selection_.selectedIndices();
    const int previousSelectedIndex = selectedIndex();
    selection_.setMode(mode);
    notifySelectionChanged(previousIndices, previousSelectedIndex);
    invalidate();
}

SelectionMode VirtualList::selectionMode() const {
    return selection_.mode();
}

void VirtualList::setSelectedIndices(std::vector<int> indices) {
    const auto previousIndices = selection_.selectedIndices();
    const int previousSelectedIndex = selectedIndex();
    selection_.setSelectedIndices(std::move(indices));
    if (selection_.activeIndex() >= 0) {
        ensureSelectionVisible();
    }
    notifySelectionChanged(previousIndices, previousSelectedIndex);
    invalidate();
}

const std::vector<int>& VirtualList::selectedIndices() const {
    return selection_.selectedIndices();
}

void VirtualList::setRowHeight(float height) {
    const float next = std::max(24.0f, height);
    if (std::fabs(next - rowHeight_) <= 0.001f) {
        return;
    }
    rowHeight_ = next;
    scrollOffset_ = std::clamp(scrollOffset_, 0.0f, maxScrollOffset());
    resetScrollMotion(scrollOffset_);
    invalidate();
}

float VirtualList::rowHeight() const {
    return rowHeight_;
}

void VirtualList::setWheelStep(float step) {
    wheelStep_ = std::max(1.0f, step);
}

void VirtualList::setScrollOffset(float offset) {
    const float next = std::clamp(offset, 0.0f, maxScrollOffset());
    resetScrollMotion(next);
    if (std::fabs(next - scrollOffset_) <= 0.001f) {
        return;
    }
    scrollOffset_ = next;
    invalidate();
}

float VirtualList::scrollOffset() const {
    return scrollOffset_;
}

float VirtualList::maxScrollOffset() const {
    return std::max(0.0f, static_cast<float>(items_.size()) * rowHeight_ - frame().height);
}

void VirtualList::setStyleOverride(ListStyleOverride style) {
    styleOverride_ = std::move(style);
    invalidate();
}

void VirtualList::clearStyleOverride() {
    styleOverride_.reset();
    invalidate();
}

void VirtualList::setOnChanged(std::function<void(int)> callback) {
    onChanged_ = std::move(callback);
}

void VirtualList::setOnSelectionChanged(std::function<void(const std::vector<int>&)> callback) {
    onSelectionChanged_ = std::move(callback);
}

void VirtualList::setOnActivated(std::function<void(int)> callback) {
    onActivated_ = std::move(callback);
}

void VirtualList::setOnEditRequested(std::function<void(int)> callback) {
    onEditRequested_ = std::move(callback);
}

void VirtualList::setOnDeleteRequested(std::function<void(const std::vector<int>&)> callback) {
    onDeleteRequested_ = std::move(callback);
}

void VirtualList::setOnContextMenuRequested(std::function<void(int, Point)> callback) {
    onContextMenuRequested_ = std::move(callback);
}

void VirtualList::setReorderEnabled(bool enabled) {
    if (reorderEnabled_ == enabled) {
        return;
    }
    reorderEnabled_ = enabled;
    resetReorderState();
    invalidate();
}

bool VirtualList::reorderEnabled() const {
    return reorderEnabled_;
}

void VirtualList::setOnReorderRequested(std::function<void(int, int)> callback) {
    onReorderRequested_ = std::move(callback);
}

bool VirtualList::setItemDragIds(std::vector<std::wstring> ids) {
    if (reorderSourceIndex_ >= 0 || reordering_ || externalDragging_) {
        return false;
    }
    if (ids.size() != items_.size()) {
        return false;
    }
    std::unordered_set<std::wstring> uniqueIds;
    uniqueIds.reserve(ids.size());
    for (auto& id : ids) {
        if (id.empty() || !uniqueIds.insert(id).second) {
            return false;
        }
    }
    itemDragIds_ = std::move(ids);
    return true;
}

void VirtualList::setItemDragEnabled(bool enabled) {
    if (itemDragEnabled_ == enabled) {
        return;
    }
    const int source = reorderSourceIndex_;
    const bool notifyCancellation = externalDragging_ && itemDragEnabled_ && onItemDrag_
        && source >= 0 && source < static_cast<int>(itemDragIds_.size());
    const ItemDragEvent cancellation{
        notifyCancellation ? itemDragIds_[static_cast<std::size_t>(source)] : std::wstring{},
        ItemDragPhase::Cancelled,
        reorderCurrentPoint_};
    const auto callback = onItemDrag_;
    itemDragEnabled_ = enabled;
    resetReorderState();
    invalidate();
    if (notifyCancellation) {
        callback(cancellation);
    }
}

bool VirtualList::itemDragEnabled() const {
    return itemDragEnabled_;
}

void VirtualList::setOnItemDrag(std::function<void(const ItemDragEvent&)> callback) {
    onItemDrag_ = std::move(callback);
}

void VirtualList::paint(Canvas& canvas) {
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
    if (items_.empty() || rect.height <= 0.0f) {
        return;
    }

    const int first = std::max(0, static_cast<int>(std::floor(scrollOffset_ / rowHeight_)) - 1);
    const int last = std::min(
        static_cast<int>(items_.size()),
        static_cast<int>(std::ceil((scrollOffset_ + rect.height) / rowHeight_)) + 1);

    canvas.save();
    canvas.clipRect(rect);
    const ListStyle normalItemStyle = resolvedItemStyle(-1);
    for (int index = first; index < last; ++index) {
        const Rect row = itemRect(index);
        canvas.save();
        canvas.clipRect(row);
        const bool hasRowState = selection_.contains(index) || index == hoveredIndex_ || index == pressedIndex_;
        std::optional<ListStyle> stateItemStyle;
        if (hasRowState) {
            stateItemStyle = resolvedItemStyle(index);
        }
        const ListStyle& itemStyle = stateItemStyle ? *stateItemStyle : normalItemStyle;
        if (index > 0) {
            canvas.drawLine(
                Point{rect.x + itemStyle.textInset, row.y},
                Point{rect.x + rect.width - itemStyle.textInset, row.y},
                itemStyle.separator,
                1.0f);
        }
        if (itemStyle.rowBackground.a > 0) {
            canvas.fillRect(row.inset(itemStyle.rowInset), itemStyle.rowBackground, itemStyle.rowRadius);
        }

        const auto& item = items_[static_cast<std::size_t>(index)];
        if (item.detail.empty()) {
            canvas.drawTextStyledEllipsized(
                item.title,
                Rect{row.x + itemStyle.textInset, row.y, row.width - itemStyle.textInset * 2.0f, row.height},
                itemStyle.titleColor,
                itemStyle.titleFontSize,
                TextAlign::Left,
                itemStyle.titleFontWeight);
        } else {
            canvas.drawTextStyledEllipsized(
                item.title,
                Rect{row.x + itemStyle.textInset, row.y + itemStyle.titleOffsetY, row.width - itemStyle.textInset * 2.0f, 18.0f},
                itemStyle.titleColor,
                itemStyle.titleFontSize,
                TextAlign::Left,
                itemStyle.titleFontWeight);
            canvas.drawTextStyledEllipsized(
                item.detail,
                Rect{row.x + itemStyle.textInset, row.y + itemStyle.detailOffsetY, row.width - itemStyle.textInset * 2.0f, 16.0f},
                itemStyle.detailColor,
                itemStyle.detailFontSize,
                TextAlign::Left,
                itemStyle.detailFontWeight);
        }
        canvas.restore();
    }
    if (reordering_ && reorderInsertionIndex_ >= 0) {
        const float rawY = rect.y
            + static_cast<float>(reorderInsertionIndex_) * rowHeight_
            - scrollOffset_;
        const float indicatorY = std::clamp(rawY, rect.y + 1.0f, rect.y + rect.height - 1.0f);
        const float indicatorInset = std::max(4.0f, normalItemStyle.textInset);
        canvas.drawLine(
            Point{rect.x + indicatorInset, indicatorY},
            Point{rect.x + rect.width - indicatorInset, indicatorY},
            containerStyle.focusRing.color,
            std::max(2.0f, containerStyle.focusRing.width));
    }
    canvas.restore();

    if (maxScrollOffset() > 0.001f) {
        const Rect thumb = verticalThumbRect(containerStyle.scrollbarWidth);
        canvas.fillRect(thumb, containerStyle.scrollbarColor, thumb.width / 2.0f);
    }
}

bool VirtualList::onMouseMove(const MouseEvent& event) {
    if (!interactive()) {
        return false;
    }
    if ((reorderEnabled_ || itemDragEnabled_) && reorderSourceIndex_ >= 0) {
        if (!reordering_ && detail::exceedsReorderDragThreshold(
                event.position.x - reorderStartPoint_.x,
                event.position.y - reorderStartPoint_.y)) {
            reordering_ = true;
            reorderCurrentPoint_ = event.position;
            if (itemDragEnabled_ && !contains(event.position)) {
                externalDragging_ = true;
                reorderTargetIndex_ = -1;
                reorderInsertionIndex_ = -1;
                invalidate();
                emitItemDrag(ItemDragPhase::Started, event.position);
                return true;
            }
        }
        if (reordering_) {
            reorderCurrentPoint_ = event.position;
            if (externalDragging_) {
                invalidate();
                emitItemDrag(ItemDragPhase::Updated, event.position);
                return true;
            }
            if (itemDragEnabled_ && !contains(event.position)) {
                externalDragging_ = true;
                reorderTargetIndex_ = -1;
                reorderInsertionIndex_ = -1;
                invalidate();
                emitItemDrag(ItemDragPhase::Started, event.position);
                return true;
            }
            if (reorderEnabled_) {
                updateReorderTarget(event.position);
            } else {
                reorderTargetIndex_ = -1;
                reorderInsertionIndex_ = -1;
            }
            hoveredIndex_ = hitItemIndex(event.position);
            invalidate();
            return true;
        }
    }
    const int next = hitItemIndex(event.position);
    if (next == hoveredIndex_) {
        return false;
    }
    hoveredIndex_ = next;
    invalidate();
    return true;
}

bool VirtualList::onMouseDown(const MouseEvent& event) {
    if (!interactive() || (event.button != MouseButton::Left && event.button != MouseButton::Right)) {
        return false;
    }
    resetReorderState();
    pressedIndex_ = hitItemIndex(event.position);
    pressedClickCount_ = event.clickCount;
    if (pressedIndex_ < 0) {
        return false;
    }
    if (event.button == MouseButton::Left
        && (reorderEnabled_ || (itemDragEnabled_ && itemDragIds_.size() == items_.size()))) {
        reorderStartPoint_ = event.position;
        reorderCurrentPoint_ = event.position;
        reorderSourceIndex_ = pressedIndex_;
        reorderTargetIndex_ = pressedIndex_;
    }
    invalidate();
    return true;
}

bool VirtualList::onMouseUp(const MouseEvent& event) {
    if (!interactive()) {
        return false;
    }
    if (event.button == MouseButton::Left && reordering_) {
        if (!externalDragging_ && reorderEnabled_ && contains(event.position)) {
            updateReorderTarget(event.position);
        } else {
            reorderTargetIndex_ = -1;
            reorderInsertionIndex_ = -1;
        }
        const int source = reorderSourceIndex_;
        const int target = reorderTargetIndex_;
        const std::wstring sourceId = source >= 0 && source < static_cast<int>(itemDragIds_.size())
            ? itemDragIds_[static_cast<std::size_t>(source)]
            : std::wstring{};
        const bool externalDrop = externalDragging_;
        const bool internalDrop = !externalDrop && reorderEnabled_ && contains(event.position);
        const auto reorderCallback = onReorderRequested_;
        const auto dragCallback = onItemDrag_;
        const ItemDragEvent dragEvent{sourceId, ItemDragPhase::Dropped, event.position};
        pressedIndex_ = -1;
        pressedClickCount_ = 1;
        resetReorderState();
        invalidate();
        if (externalDrop) {
            if (itemDragEnabled_ && !sourceId.empty() && dragCallback) {
                dragCallback(dragEvent);
            }
        } else if (internalDrop) {
            if (source >= 0 && target >= 0 && source != target && reorderCallback) {
                reorderCallback(source, target);
            }
        }
        return true;
    }
    const int pressed = pressedIndex_;
    const int clickCount = pressedClickCount_;
    pressedIndex_ = -1;
    pressedClickCount_ = 1;
    resetReorderState();
    if (pressed < 0) {
        return false;
    }
    if (hitItemIndex(event.position) == pressed) {
        const auto previousIndices = selection_.selectedIndices();
        const int previousSelectedIndex = selectedIndex();
        if (event.button == MouseButton::Right) {
            if (!selection_.contains(pressed)) {
                selection_.selectOnly(pressed);
            }
        } else {
            selection_.applyPointerSelection(pressed, event.control, event.shift);
        }
        ensureSelectionVisible();
        notifySelectionChanged(previousIndices, previousSelectedIndex);
        if (event.button == MouseButton::Right && onContextMenuRequested_) {
            onContextMenuRequested_(pressed, event.position);
        } else if (event.button == MouseButton::Left && clickCount == 2 && onActivated_) {
            onActivated_(pressed);
        }
    }
    invalidate();
    return true;
}

bool VirtualList::onMouseWheel(const MouseWheelEvent& event) {
    if (!interactive() || !contains(event.position) || maxScrollOffset() <= 0.001f) {
        return false;
    }
    const double nowMs = event.timestampMs > 0.0 ? event.timestampMs : currentTimeMs();
    const double wheelIntervalMs = scrollTraceLastWheelMs_ > 0.0
        ? nowMs - scrollTraceLastWheelMs_
        : 0.0;
    scrollTraceLastWheelMs_ = nowMs;
    if (internal::scrollTraceEnabled()) {
        internal::writeScrollTrace(internal::ScrollTraceEvent{
            "virtual_list", "wheel_enter", reinterpret_cast<std::uintptr_t>(this),
            event.deltaY, scrollOffset_, scrollMotion_.target(), scrollMotion_.velocity(),
            wheelIntervalMs, maxScrollOffset(), 0.0, wheelStep_,
            scrollMotion_.running() ? 1.0 : 0.0});
    }

    const bool caughtUp = advanceScrollMotion(nowMs);
    const float inputDistance = -event.deltaY * wheelStep_;
    const bool accepted = scrollMotion_.addDelta(
        inputDistance,
        0.0f,
        maxScrollOffset(),
        nowMs,
        kDefaultWheelScrollMotionSpec);
    if (accepted || scrollMotion_.running()) {
        requestAnimationFrame();
    }
    if (internal::scrollTraceEnabled()) {
        internal::writeScrollTrace(internal::ScrollTraceEvent{
            "virtual_list", "wheel_applied", reinterpret_cast<std::uintptr_t>(this),
            event.deltaY, scrollOffset_, scrollMotion_.target(), scrollMotion_.velocity(),
            wheelIntervalMs, maxScrollOffset(),
            kDefaultWheelScrollMotionSpec.retargetSettlingDurationMs,
            inputDistance, accepted ? 1.0 : 0.0});
    }
    return accepted || caughtUp;
}

bool VirtualList::tickAnimations(double nowMs) {
    const double frameIntervalMs = scrollTraceLastTickMs_ > 0.0
        ? nowMs - scrollTraceLastTickMs_
        : 0.0;
    scrollTraceLastTickMs_ = nowMs;
    const float previousOffset = scrollOffset_;
    const bool ticked = advanceScrollMotion(nowMs);
    if (internal::scrollTraceEnabled() && ticked) {
        internal::writeScrollTrace(internal::ScrollTraceEvent{
            "virtual_list", "animation_tick", reinterpret_cast<std::uintptr_t>(this),
            0.0, scrollOffset_, scrollMotion_.target(), scrollMotion_.velocity(),
            frameIntervalMs, maxScrollOffset(), 0.0,
            scrollOffset_ - previousOffset, 0.0});
    }
    if (scrollMotion_.running()) {
        requestAnimationFrame();
    }
    return ticked;
}

bool VirtualList::onKeyDown(const KeyEvent& event) {
    if (!interactive() || items_.empty()) {
        return false;
    }
    if (event.key == Key::A && event.control && selection_.mode() == SelectionMode::Multiple) {
        const auto previousIndices = selection_.selectedIndices();
        const int previousSelectedIndex = selectedIndex();
        selection_.selectAll();
        notifySelectionChanged(previousIndices, previousSelectedIndex);
        invalidate();
        return true;
    }
    const int active = selection_.activeIndex();
    if (reorderEnabled_ && event.alt && !event.control && active >= 0
        && (event.key == Key::Up || event.key == Key::Down)) {
        const int target = event.key == Key::Up
            ? std::max(0, active - 1)
            : std::min(static_cast<int>(items_.size()) - 1, active + 1);
        const auto callback = onReorderRequested_;
        if (target != active && callback) {
            callback(active, target);
        }
        return true;
    }
    if (active >= 0 && event.key == Key::Enter && onActivated_) {
        onActivated_(active);
        return true;
    }
    if (active >= 0 && event.key == Key::F2 && onEditRequested_) {
        onEditRequested_(active);
        return true;
    }
    if (active >= 0 && event.key == Key::Delete && onDeleteRequested_) {
        onDeleteRequested_(selection_.selectedIndices());
        return true;
    }
    const int selected = active >= 0 ? active : -1;
    int target = -1;
    if (event.key == Key::Down) {
        target = std::min(static_cast<int>(items_.size()) - 1, selected + 1);
    } else if (event.key == Key::Up) {
        target = selected < 0 ? 0 : std::max(0, selected - 1);
    } else if (event.key == Key::Home) {
        target = 0;
    } else if (event.key == Key::End) {
        target = static_cast<int>(items_.size()) - 1;
    } else if (event.key == Key::PageDown || event.key == Key::PageUp) {
        const int visibleRows = std::max(
            1,
            static_cast<int>(std::floor(frame().height / rowHeight_)));
        const int origin = selected < 0 ? 0 : selected;
        target = event.key == Key::PageDown
            ? std::min(static_cast<int>(items_.size()) - 1, origin + visibleRows)
            : std::max(0, origin - visibleRows);
    }
    if (target >= 0) {
        const auto previousIndices = selection_.selectedIndices();
        const int previousSelectedIndex = selectedIndex();
        selection_.applyKeyboardSelection(target, event.control, event.shift);
        ensureSelectionVisible();
        notifySelectionChanged(previousIndices, previousSelectedIndex);
        invalidate();
        return true;
    }
    return false;
}

bool VirtualList::isFocusable() const {
    return interactive() && !items_.empty();
}

AccessibilityInfo VirtualList::accessibilityInfo() const {
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

void VirtualList::assignSelectedIndex(int index) {
    const auto previousIndices = selection_.selectedIndices();
    const int previousSelectedIndex = selectedIndex();
    if (items_.empty()) {
        selection_.clear();
        scrollOffset_ = 0.0f;
        notifySelectionChanged(previousIndices, previousSelectedIndex);
        invalidate();
        return;
    }
    if (index < 0) {
        selection_.clear();
    } else {
        selection_.selectOnly(std::clamp(index, 0, static_cast<int>(items_.size()) - 1));
    }
    if (selection_.activeIndex() >= 0) {
        ensureSelectionVisible();
    }
    notifySelectionChanged(previousIndices, previousSelectedIndex);
    invalidate();
}

void VirtualList::notifySelectionChanged(
    const std::vector<int>& previousIndices,
    int previousSelectedIndex) {
    if (previousIndices == selection_.selectedIndices()) {
        return;
    }
    const int nextSelectedIndex = selectedIndex();
    if (previousSelectedIndex != nextSelectedIndex && onChanged_) {
        onChanged_(nextSelectedIndex);
    }
    if (onSelectionChanged_) {
        onSelectionChanged_(selection_.selectedIndices());
    }
}

int VirtualList::effectiveSelectedIndex() const {
    if (items_.empty()) {
        return -1;
    }
    return selection_.activeIndex();
}

int VirtualList::hitItemIndex(Point point) const {
    if (items_.empty() || !contains(point)) {
        return -1;
    }
    const int index = static_cast<int>((point.y - frame().y + scrollOffset_) / rowHeight_);
    return index >= 0 && index < static_cast<int>(items_.size()) ? index : -1;
}

Rect VirtualList::itemRect(int index) const {
    const Rect rect = frame();
    return Rect{rect.x, rect.y + static_cast<float>(index) * rowHeight_ - scrollOffset_, rect.width, rowHeight_};
}

Rect VirtualList::verticalThumbRect(float width) const {
    const Rect rect = frame();
    const float thumbWidth = std::max(1.0f, width);
    const float contentHeight = static_cast<float>(items_.size()) * rowHeight_;
    const float thumbHeight = std::max(24.0f, rect.height * rect.height / contentHeight);
    const float travel = std::max(0.0f, rect.height - thumbHeight - 10.0f);
    const float progress = maxScrollOffset() <= 0.001f ? 0.0f : scrollOffset_ / maxScrollOffset();
    return Rect{rect.x + rect.width - thumbWidth - 4.0f, rect.y + 5.0f + progress * travel, thumbWidth, thumbHeight};
}

void VirtualList::resetReorderState() {
    reordering_ = false;
    externalDragging_ = false;
    reorderStartPoint_ = {};
    reorderCurrentPoint_ = {};
    reorderSourceIndex_ = -1;
    reorderTargetIndex_ = -1;
    reorderInsertionIndex_ = -1;
}

void VirtualList::emitItemDrag(ItemDragPhase phase, Point position) {
    if (!itemDragEnabled_ || !onItemDrag_ || reorderSourceIndex_ < 0
        || reorderSourceIndex_ >= static_cast<int>(itemDragIds_.size())) {
        return;
    }
    onItemDrag_(ItemDragEvent{
        itemDragIds_[static_cast<std::size_t>(reorderSourceIndex_)], phase, position});
}

void VirtualList::updateReorderTarget(Point point) {
    reorderInsertionIndex_ = detail::reorderInsertionIndex(
        point.y,
        frame().y,
        scrollOffset_,
        rowHeight_,
        static_cast<int>(items_.size()));
    reorderTargetIndex_ = detail::reorderTargetIndex(
        reorderSourceIndex_,
        reorderInsertionIndex_,
        static_cast<int>(items_.size()));
}

void VirtualList::ensureSelectionVisible() {
    const int selected = effectiveSelectedIndex();
    if (selected < 0 || frame().height <= 0.001f) {
        return;
    }
    const float top = static_cast<float>(selected) * rowHeight_;
    const float bottom = top + rowHeight_;
    if (top < scrollOffset_) {
        scrollOffset_ = top;
    } else if (bottom > scrollOffset_ + frame().height) {
        scrollOffset_ = bottom - frame().height;
    }
    scrollOffset_ = std::clamp(scrollOffset_, 0.0f, maxScrollOffset());
    resetScrollMotion(scrollOffset_);
}

void VirtualList::resetScrollMotion(float offset) {
    scrollMotion_.reset(offset);
}

bool VirtualList::advanceScrollMotion(double nowMs) {
    if (!scrollMotion_.running()) {
        return false;
    }

    const float previous = scrollOffset_;
    scrollMotion_.tick(nowMs);
    scrollOffset_ = std::clamp(scrollMotion_.value(), 0.0f, maxScrollOffset());
    if (std::fabs(scrollOffset_ - previous) > 0.001f) {
        invalidate();
    }
    return true;
}

ListStyle VirtualList::resolvedContainerStyle() const {
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

ListStyle VirtualList::resolvedItemStyle(int index) const {
    const bool hasItem = index >= 0;
    const bool selected = hasItem && selection_.contains(index);
    const bool hovered = hasItem && index == hoveredIndex_;
    const bool pressed = hasItem && index == pressedIndex_;
    ListStyle style = detail::baseListStyle(selected, disabled(), hovered, pressed);
    if (!styleOverride_) {
        return style;
    }
    if (styleOverride_->normal) {
        detail::applyListStateOverride(style, *styleOverride_->normal);
    }
    if (selected) {
        style.rowBackground = style.selectedRowBackground;
        style.titleColor = style.selectedTitleColor;
        style.detailColor = style.selectedDetailColor;
    }
    if (disabled() && styleOverride_->disabled) {
        detail::applyListStateOverride(style, *styleOverride_->disabled);
    } else if (pressed && styleOverride_->pressed) {
        detail::applyListStateOverride(style, *styleOverride_->pressed);
    } else if (selected && styleOverride_->selected) {
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

bool VirtualList::hasInteractionState() const {
    return hoveredIndex_ >= 0 || pressedIndex_ >= 0;
}

void VirtualList::resetInteractionState() {
    const int source = reorderSourceIndex_;
    const bool notifyCancellation = externalDragging_ && itemDragEnabled_ && onItemDrag_
        && source >= 0 && source < static_cast<int>(itemDragIds_.size());
    const ItemDragEvent cancellation{
        notifyCancellation ? itemDragIds_[static_cast<std::size_t>(source)] : std::wstring{},
        ItemDragPhase::Cancelled,
        reorderCurrentPoint_};
    const auto callback = onItemDrag_;
    hoveredIndex_ = -1;
    pressedIndex_ = -1;
    pressedClickCount_ = 1;
    resetReorderState();
    if (notifyCancellation) {
        callback(cancellation);
    }
}

} // namespace oneui

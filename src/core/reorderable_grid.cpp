#include "oneui/layout/reorderable_grid.h"

#include "reorder_internal.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace oneui {

ReorderableGrid::ReorderableGrid() {
    setPreferredSize(Size{320.0f, itemHeight_});
}

void ReorderableGrid::setItems(std::vector<ReorderableGridItem> items) {
    clearChildren();
    items_.clear();
    itemIds_.clear();
    items_.reserve(items.size());
    itemIds_.reserve(items.size());
    for (auto& item : items) {
        if (!item.content || item.id.empty() || !itemIds_.insert(item.id).second) {
            continue;
        }
        add(item.content);
        items_.push_back(std::move(item));
    }
    resetReorderState();
    updatePreferredHeight();
    invalidate();
}

void ReorderableGrid::clearItems() {
    setItems({});
}

void ReorderableGrid::addItem(ReorderableGridItem item) {
    if (!item.content || item.id.empty() || !itemIds_.insert(item.id).second) {
        return;
    }
    add(item.content);
    items_.push_back(std::move(item));
    resetReorderState();
    updatePreferredHeight();
    invalidate();
}

bool ReorderableGrid::moveItem(const std::wstring& sourceId, int targetIndex) {
    const auto source = std::find_if(
        items_.begin(), items_.end(), [&](const ReorderableGridItem& item) {
            return item.id == sourceId;
        });
    if (source == items_.end() || targetIndex < 0 || targetIndex >= itemCount()) {
        return false;
    }
    const int sourceIndex = static_cast<int>(source - items_.begin());
    if (sourceIndex == targetIndex) {
        return false;
    }

    ReorderableGridItem moved = std::move(*source);
    items_.erase(source);
    items_.insert(items_.begin() + targetIndex, std::move(moved));
    clearChildren();
    for (const auto& item : items_) {
        add(item.content);
    }
    resetReorderState();
    invalidate();
    return true;
}

int ReorderableGrid::itemCount() const {
    return static_cast<int>(items_.size());
}

void ReorderableGrid::setColumnCount(int columns) {
    const int next = std::max(1, columns);
    if (next == columns_) {
        return;
    }
    columns_ = next;
    resetReorderState();
    updatePreferredHeight();
    invalidate();
}

int ReorderableGrid::columnCount() const {
    return columns_;
}

void ReorderableGrid::setColumnGap(float gap) {
    columnGap_ = std::max(0.0f, gap);
    invalidate();
}

void ReorderableGrid::setRowGap(float gap) {
    rowGap_ = std::max(0.0f, gap);
    updatePreferredHeight();
    invalidate();
}

void ReorderableGrid::setItemHeight(float height) {
    itemHeight_ = std::max(1.0f, height);
    updatePreferredHeight();
    invalidate();
}

float ReorderableGrid::contentHeight() const {
    const int rows = items_.empty()
        ? 0
        : (static_cast<int>(items_.size()) + columns_ - 1) / columns_;
    const Insets insets = padding();
    return insets.top + insets.bottom
        + static_cast<float>(rows) * resolvedItemHeight()
        + static_cast<float>(std::max(0, rows - 1)) * resolvedRowGap();
}

void ReorderableGrid::setStyleBox(StyleBox style) {
    style_ = std::move(style);
    updatePreferredHeight();
    invalidate();
}

void ReorderableGrid::setReorderEnabled(bool enabled) {
    if (reorderEnabled_ == enabled) {
        return;
    }
    reorderEnabled_ = enabled;
    resetReorderState();
    invalidate();
}

bool ReorderableGrid::reorderEnabled() const {
    return reorderEnabled_;
}

void ReorderableGrid::setOnReorderRequested(
    std::function<void(const std::wstring&, int)> callback) {
    onReorderRequested_ = std::move(callback);
}

void ReorderableGrid::paint(Canvas& canvas) {
    View::paint(canvas);
    if (!reordering_ || reorderInsertionIndex_ < 0 || items_.empty()) {
        return;
    }

    const int referenceIndex = reorderInsertionIndex_ >= itemCount()
        ? itemCount() - 1
        : reorderInsertionIndex_;
    const Rect reference = items_[static_cast<std::size_t>(referenceIndex)].content->frame();
    const float columnGap = resolvedColumnGap();
    float x = reorderInsertionIndex_ >= itemCount()
        ? reference.x + reference.width + columnGap / 2.0f
        : reference.x - columnGap / 2.0f;
    x = std::clamp(x, frame().x + 1.0f, frame().x + frame().width - 1.0f);
    canvas.drawLine(
        Point{x, reference.y + 4.0f},
        Point{x, reference.y + reference.height - 4.0f},
        indicatorColor(),
        indicatorWidth());
}

bool ReorderableGrid::onMouseMove(const MouseEvent& event) {
    if (!interactive()) {
        return false;
    }
    if (reorderEnabled_ && reorderSourceIndex_ >= 0) {
        if (!reordering_ && detail::exceedsReorderDragThreshold(
                event.position.x - reorderStartPoint_.x,
                event.position.y - reorderStartPoint_.y)) {
            reordering_ = true;
            View::resetInteractionState();
        }
        if (reordering_) {
            updateReorderTarget(event.position);
            invalidate();
            return true;
        }
    }
    return View::onMouseMove(event);
}

bool ReorderableGrid::onMouseDown(const MouseEvent& event) {
    if (!interactive()) {
        return false;
    }
    resetReorderState();
    if (event.button == MouseButton::Left && reorderEnabled_) {
        reorderSourceIndex_ = itemIndexAt(event.position);
        if (reorderSourceIndex_ >= 0) {
            reorderStartPoint_ = event.position;
            reorderTargetIndex_ = reorderSourceIndex_;
            reorderInsertionIndex_ = reorderSourceIndex_;
        }
    }
    const bool handled = View::onMouseDown(event);
    return handled || reorderSourceIndex_ >= 0;
}

bool ReorderableGrid::onMouseUp(const MouseEvent& event) {
    if (!interactive()) {
        return false;
    }
    if (event.button == MouseButton::Left && reordering_) {
        updateReorderTarget(event.position);
        const int source = reorderSourceIndex_;
        const int target = reorderTargetIndex_;
        const std::wstring sourceId = source >= 0 && source < itemCount()
            ? items_[static_cast<std::size_t>(source)].id
            : std::wstring{};
        const auto callback = onReorderRequested_;
        resetReorderState();
        invalidate();
        if (!sourceId.empty() && target >= 0 && source != target && callback) {
            callback(sourceId, target);
        }
        return true;
    }

    // A child activation may synchronously replace and destroy this grid.
    // Clear local gesture state before crossing that callback boundary.
    resetReorderState();
    return View::onMouseUp(event);
}

void ReorderableGrid::layoutChildren() {
    const Rect content = frame().inset(padding());
    const float columnGap = resolvedColumnGap();
    const float rowGap = resolvedRowGap();
    const float itemHeight = resolvedItemHeight();
    const float totalGap = static_cast<float>(std::max(0, columns_ - 1)) * columnGap;
    const float itemWidth = std::max(
        0.0f,
        (content.width - totalGap) / static_cast<float>(columns_));
    for (int index = 0; index < itemCount(); ++index) {
        const int row = index / columns_;
        const int column = index % columns_;
        items_[static_cast<std::size_t>(index)].content->setFrame(Rect{
            content.x + static_cast<float>(column) * (itemWidth + columnGap),
            content.y + static_cast<float>(row) * (itemHeight + rowGap),
            itemWidth,
            itemHeight});
    }
}

void ReorderableGrid::resetInteractionState() {
    View::resetInteractionState();
    resetReorderState();
}

int ReorderableGrid::itemIndexAt(Point point) const {
    for (int index = itemCount() - 1; index >= 0; --index) {
        const auto& item = items_[static_cast<std::size_t>(index)];
        if (item.content->visible() && item.content->hitTest(point)) {
            return index;
        }
    }
    return -1;
}

int ReorderableGrid::insertionIndexAt(Point point) const {
    if (items_.empty()) {
        return 0;
    }
    const Rect content = frame().inset(padding());
    if (point.y <= content.y) {
        return 0;
    }
    const int rowCount = (itemCount() + columns_ - 1) / columns_;
    const float rowStride = resolvedItemHeight() + resolvedRowGap();
    int row = rowStride > 0.0f
        ? static_cast<int>(std::floor((point.y - content.y) / rowStride))
        : 0;
    row = std::clamp(row, 0, rowCount - 1);
    if (point.y >= frame().y + contentHeight() - padding().bottom) {
        return itemCount();
    }

    const int start = row * columns_;
    const int end = std::min(itemCount(), start + columns_);
    for (int index = start; index < end; ++index) {
        const Rect item = items_[static_cast<std::size_t>(index)].content->frame();
        if (point.x < item.x + item.width / 2.0f) {
            return index;
        }
    }
    return end;
}

void ReorderableGrid::updateReorderTarget(Point point) {
    reorderInsertionIndex_ = insertionIndexAt(point);
    reorderTargetIndex_ = detail::reorderTargetIndex(
        reorderSourceIndex_, reorderInsertionIndex_, itemCount());
}

void ReorderableGrid::resetReorderState() {
    reordering_ = false;
    reorderStartPoint_ = {};
    reorderSourceIndex_ = -1;
    reorderTargetIndex_ = -1;
    reorderInsertionIndex_ = -1;
}

void ReorderableGrid::updatePreferredHeight() {
    const Size current = preferredSize();
    setPreferredSize(Size{current.width, contentHeight()});
}

Insets ReorderableGrid::padding() const {
    return style_.padding.value_or(Insets{});
}

float ReorderableGrid::resolvedColumnGap() const {
    return std::max(0.0f, style_.gap.value_or(columnGap_));
}

float ReorderableGrid::resolvedRowGap() const {
    return std::max(0.0f, style_.gap.value_or(rowGap_));
}

float ReorderableGrid::resolvedItemHeight() const {
    return std::max(1.0f, style_.height.value_or(itemHeight_));
}

Color ReorderableGrid::indicatorColor() const {
    return style_.outlineColor
        .value_or(style_.borderColor.value_or(Color{99, 102, 241, 255}));
}

float ReorderableGrid::indicatorWidth() const {
    return std::max(1.0f, style_.outlineWidth.value_or(2.0f));
}

} // namespace oneui

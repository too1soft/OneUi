#include "oneui/controls/table.h"

#include "oneui/style.h"

#include "reorder_internal.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <unordered_set>
#include <utility>

namespace oneui {
namespace {

double currentTimeMs() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration<double, std::milli>(now).count();
}

void applyTableStyleOverride(TableStyle& style, const TableStyleOverride& override) {
    if (override.background) style.background = *override.background;
    if (override.border) style.border = *override.border;
    if (override.headerBackground) style.headerBackground = *override.headerBackground;
    if (override.headerForeground) style.headerForeground = *override.headerForeground;
    if (override.cellForeground) style.cellForeground = *override.cellForeground;
    if (override.gridLine) style.gridLine = *override.gridLine;
    if (override.rowHovered) style.rowHovered = *override.rowHovered;
    if (override.rowPressed) style.rowPressed = *override.rowPressed;
    if (override.rowSelected) style.rowSelected = *override.rowSelected;
    if (override.scrollbarColor) style.scrollbarColor = *override.scrollbarColor;
    if (override.borderWidth) style.borderWidth = *override.borderWidth;
    if (override.radius) style.radius = *override.radius;
    if (override.headerHeight) style.headerHeight = *override.headerHeight;
    if (override.scrollbarWidth) style.scrollbarWidth = *override.scrollbarWidth;
    if (override.cellPadding) style.cellPadding = *override.cellPadding;
}

} // namespace

Table::Table() {
    setPreferredSize(Size{640.0f, 320.0f});
}

void Table::setColumns(std::vector<TableColumn> columns) {
    columns_ = std::move(columns);
    invalidate();
}

void Table::setRows(std::vector<std::vector<std::wstring>> rows) {
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
    rows_ = std::move(rows);
    itemDragIds_.clear();
    selection_.setItemCount(static_cast<int>(rows_.size()));
    if (wasUninitialized && !rows_.empty()) selection_.selectOnly(0);
    hoveredIndex_ = -1;
    pressedIndex_ = -1;
    pressedClickCount_ = 1;
    resetReorderState();
    scrollOffset_ = std::clamp(scrollOffset_, 0.0f, maxScrollOffset());
    resetScrollMotion(scrollOffset_);
    ensureSelectionVisible();
    notifySelectionChanged(previousIndices, previousSelectedIndex);
    invalidate();
    if (notifyCancellation) dragCallback(cancellation);
}

bool Table::updateRow(std::size_t index, std::vector<std::wstring> row) {
    if (index >= rows_.size()) return false;
    if (rows_[index] == row) return true;
    rows_[index] = std::move(row);
    invalidate();
    return true;
}

const std::vector<TableColumn>& Table::columns() const { return columns_; }
const std::vector<std::vector<std::wstring>>& Table::rows() const { return rows_; }

Rect Table::rowFrame(int index) const {
    const TableStyle style = resolvedStyle();
    const float contentHeight = std::max(0.0f, frame().height - style.headerHeight);
    return rowRect(index, effectiveRowHeight(contentHeight));
}

void Table::setSelectionMode(SelectionMode mode) {
    const auto previousIndices = selection_.selectedIndices();
    const int previousSelectedIndex = selectedIndex();
    selection_.setMode(mode);
    notifySelectionChanged(previousIndices, previousSelectedIndex);
    invalidate();
}

void Table::setSelectedIndex(int index) {
    const auto previousIndices = selection_.selectedIndices();
    const int previousSelectedIndex = selectedIndex();
    if (rows_.empty() || index < 0) selection_.clear();
    else selection_.selectOnly(std::clamp(index, 0, static_cast<int>(rows_.size()) - 1));
    ensureSelectionVisible();
    notifySelectionChanged(previousIndices, previousSelectedIndex);
    invalidate();
}

int Table::selectedIndex() const {
    const int active = selection_.activeIndex();
    if (selection_.contains(active)) return active;
    return selection_.selectedIndices().empty() ? -1 : selection_.selectedIndices().back();
}

void Table::setSelectedIndices(std::vector<int> indices) {
    const auto previousIndices = selection_.selectedIndices();
    const int previousSelectedIndex = selectedIndex();
    selection_.setSelectedIndices(std::move(indices));
    ensureSelectionVisible();
    notifySelectionChanged(previousIndices, previousSelectedIndex);
    invalidate();
}

const std::vector<int>& Table::selectedIndices() const { return selection_.selectedIndices(); }

void Table::setRowHeight(float height) {
    const float next = height <= 0.0f ? 0.0f : std::max(24.0f, height);
    if (std::fabs(next - rowHeight_) <= 0.001f) return;
    rowHeight_ = next;
    scrollOffset_ = std::clamp(scrollOffset_, 0.0f, maxScrollOffset());
    resetScrollMotion(scrollOffset_);
    invalidate();
}

float Table::rowHeight() const { return rowHeight_; }
void Table::setWheelStep(float step) { wheelStep_ = std::max(1.0f, step); }

void Table::setScrollOffset(float offset) {
    const float next = std::clamp(offset, 0.0f, maxScrollOffset());
    resetScrollMotion(next);
    if (std::fabs(next - scrollOffset_) <= 0.001f) return;
    scrollOffset_ = next;
    invalidate();
}

float Table::scrollOffset() const { return scrollOffset_; }

float Table::maxScrollOffset() const {
    const TableStyle style = resolvedStyle();
    const float contentHeight = std::max(0.0f, frame().height - std::max(1.0f, style.headerHeight));
    return std::max(0.0f, static_cast<float>(rows_.size()) * effectiveRowHeight(contentHeight) - contentHeight);
}

void Table::setOnChanged(std::function<void(int)> callback) { onChanged_ = std::move(callback); }
void Table::setOnSelectionChanged(std::function<void(const std::vector<int>&)> callback) { onSelectionChanged_ = std::move(callback); }
void Table::setOnActivated(std::function<void(int)> callback) { onActivated_ = std::move(callback); }
void Table::setOnEditRequested(std::function<void(int)> callback) { onEditRequested_ = std::move(callback); }
void Table::setOnDeleteRequested(std::function<void(const std::vector<int>&)> callback) { onDeleteRequested_ = std::move(callback); }
void Table::setOnContextMenuRequested(std::function<void(int, Point)> callback) { onContextMenuRequested_ = std::move(callback); }

void Table::setReorderEnabled(bool enabled) {
    if (reorderEnabled_ == enabled) return;
    reorderEnabled_ = enabled;
    resetReorderState();
    invalidate();
}

bool Table::reorderEnabled() const { return reorderEnabled_; }

void Table::setOnReorderRequested(std::function<void(int, int)> callback) {
    onReorderRequested_ = std::move(callback);
}

bool Table::setItemDragIds(std::vector<std::wstring> ids) {
    if (reorderSourceIndex_ >= 0 || reordering_ || externalDragging_) return false;
    if (ids.size() != rows_.size()) return false;
    std::unordered_set<std::wstring> uniqueIds;
    uniqueIds.reserve(ids.size());
    for (auto& id : ids) {
        if (id.empty() || !uniqueIds.insert(id).second) return false;
    }
    itemDragIds_ = std::move(ids);
    return true;
}

void Table::setItemDragEnabled(bool enabled) {
    if (itemDragEnabled_ == enabled) return;
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
    if (notifyCancellation) callback(cancellation);
}

bool Table::itemDragEnabled() const { return itemDragEnabled_; }

void Table::setOnItemDrag(std::function<void(const ItemDragEvent&)> callback) {
    onItemDrag_ = std::move(callback);
}

void Table::setStyleOverride(TableStyleOverride style) {
    styleOverride_ = std::move(style);
    invalidate();
}

void Table::clearStyleOverride() {
    styleOverride_.reset();
    invalidate();
}

void Table::paint(Canvas& canvas) {
    const TableStyle style = resolvedStyle();
    const Rect rect = frame();
    canvas.fillRect(rect, style.background, style.radius);
    canvas.strokeRect(rect, style.border, style.radius, style.borderWidth);
    if (focusVisible() && !disabled()) {
        canvas.strokeRect(
            Rect{rect.x - 2.0f, rect.y - 2.0f, rect.width + 4.0f, rect.height + 4.0f},
            theme().focusOutline,
            style.radius + 2.0f,
            1.0f);
    }
    if (columns_.empty()) return;

    const float headerHeight = std::max(1.0f, style.headerHeight);
    const Rect headerRect{rect.x, rect.y, rect.width, std::min(rect.height, headerHeight)};
    const Rect contentRect{rect.x, rect.y + headerHeight, rect.width, std::max(0.0f, rect.height - headerHeight)};
    const float height = effectiveRowHeight(contentRect.height);
    canvas.fillRect(headerRect, style.headerBackground, style.radius);
    canvas.drawLine(Point{rect.x, rect.y + headerHeight}, Point{rect.x + rect.width, rect.y + headerHeight}, style.gridLine, 1.0f);

    float fixedWidth = 0.0f;
    int flexibleCount = 0;
    for (const auto& column : columns_) {
        if (column.width > 0.0f) fixedWidth += column.width;
        else ++flexibleCount;
    }
    const float remainingWidth = std::max(0.0f, rect.width - fixedWidth);
    float x = rect.x;
    for (int columnIndex = 0; columnIndex < static_cast<int>(columns_.size()); ++columnIndex) {
        const float width = columnWidth(columnIndex, remainingWidth, flexibleCount);
        if (columnIndex > 0) {
            canvas.drawLine(Point{x, rect.y}, Point{x, rect.y + rect.height}, style.gridLine, 1.0f);
        }
        canvas.drawTextEllipsized(
            columns_[static_cast<std::size_t>(columnIndex)].header,
            Rect{x, rect.y, width, headerHeight}.inset(style.cellPadding),
            style.headerForeground, theme().fontSm, TextAlign::Left);
        x += width;
    }

    if (rows_.empty() || contentRect.height <= 0.0f || height <= 0.0f) return;
    const int first = std::max(0, static_cast<int>(std::floor(scrollOffset_ / height)) - 1);
    const int last = std::min(
        static_cast<int>(rows_.size()),
        static_cast<int>(std::ceil((scrollOffset_ + contentRect.height) / height)) + 1);
    canvas.save();
    canvas.clipRect(contentRect);
    for (int rowIndex = first; rowIndex < last; ++rowIndex) {
        const Rect row = rowRect(rowIndex, height);
        if (selection_.contains(rowIndex)) canvas.fillRect(row, style.rowSelected, 0.0f);
        else if (rowIndex == pressedIndex_) canvas.fillRect(row, style.rowPressed, 0.0f);
        else if (rowIndex == hoveredIndex_) canvas.fillRect(row, style.rowHovered, 0.0f);
        if (rowIndex > 0) {
            canvas.drawLine(Point{rect.x, row.y}, Point{rect.x + rect.width, row.y}, style.gridLine, 1.0f);
        }

        x = rect.x;
        const auto& values = rows_[static_cast<std::size_t>(rowIndex)];
        for (int columnIndex = 0; columnIndex < static_cast<int>(columns_.size()); ++columnIndex) {
            const float width = columnWidth(columnIndex, remainingWidth, flexibleCount);
            const std::wstring empty;
            const auto& text = columnIndex < static_cast<int>(values.size())
                ? values[static_cast<std::size_t>(columnIndex)] : empty;
            const bool usageColumn = columns_[static_cast<std::size_t>(columnIndex)].header == L"使用率"
                && !text.empty() && text.back() == L'%';
            if (usageColumn) {
                canvas.drawTextEllipsized(
                    text,
                    Rect{x, row.y, std::min(54.0f, width), row.height}.inset(style.cellPadding),
                    style.cellForeground,
                    theme().fontMd,
                    TextAlign::Left);
                float percentage = 0.0f;
                try {
                    percentage = std::clamp(std::stof(text), 0.0f, 100.0f);
                } catch (...) {
                    percentage = 0.0f;
                }
                const float barX = x + std::min(64.0f, width * 0.34f);
                const float barWidth = std::max(0.0f, width - (barX - x) - 12.0f);
                const Rect track{barX, row.y + row.height * 0.5f - 2.0f, barWidth, 4.0f};
                canvas.fillRect(track, Color{31, 40, 50}, 1.0f);
                canvas.fillRect(
                    Rect{track.x, track.y, track.width * percentage / 100.0f, track.height},
                    theme().warning,
                    1.0f);
                x += width;
                continue;
            }
            canvas.drawTextEllipsized(
                text, Rect{x, row.y, width, row.height}.inset(style.cellPadding),
                style.cellForeground, theme().fontMd, TextAlign::Left);
            x += width;
        }
    }
    if (reordering_ && reorderInsertionIndex_ >= 0) {
        const float rawY = contentRect.y
            + static_cast<float>(reorderInsertionIndex_) * height
            - scrollOffset_;
        const float indicatorY = std::clamp(
            rawY,
            contentRect.y + 1.0f,
            contentRect.y + contentRect.height - 1.0f);
        const float indicatorInset = std::max(4.0f, style.cellPadding.left);
        canvas.drawLine(
            Point{contentRect.x + indicatorInset, indicatorY},
            Point{contentRect.x + contentRect.width - indicatorInset, indicatorY},
            theme().focusOutline,
            2.0f);
    }
    canvas.restore();

    if (maxScrollOffset() > 0.001f) {
        const Rect thumb = verticalThumbRect(style.scrollbarWidth, contentRect.height);
        canvas.fillRect(thumb, style.scrollbarColor, thumb.width / 2.0f);
    }
}

bool Table::onMouseMove(const MouseEvent& event) {
    if (!interactive()) return false;
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
            if (reorderEnabled_) updateReorderTarget(event.position);
            else {
                reorderTargetIndex_ = -1;
                reorderInsertionIndex_ = -1;
            }
            hoveredIndex_ = hitRowIndex(event.position);
            invalidate();
            return true;
        }
    }
    const int next = hitRowIndex(event.position);
    if (next == hoveredIndex_) return false;
    hoveredIndex_ = next;
    invalidate();
    return true;
}

bool Table::onMouseDown(const MouseEvent& event) {
    if (!interactive() || (event.button != MouseButton::Left && event.button != MouseButton::Right)) return false;
    resetReorderState();
    pressedIndex_ = hitRowIndex(event.position);
    pressedClickCount_ = event.clickCount;
    if (pressedIndex_ < 0) return false;
    if (event.button == MouseButton::Left
        && (reorderEnabled_ || (itemDragEnabled_ && itemDragIds_.size() == rows_.size()))) {
        reorderStartPoint_ = event.position;
        reorderCurrentPoint_ = event.position;
        reorderSourceIndex_ = pressedIndex_;
        reorderTargetIndex_ = pressedIndex_;
    }
    invalidate();
    return true;
}

bool Table::onMouseUp(const MouseEvent& event) {
    if (!interactive()) return false;
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
            if (itemDragEnabled_ && !sourceId.empty() && dragCallback) dragCallback(dragEvent);
        } else if (internalDrop && source >= 0 && target >= 0 && source != target && reorderCallback) {
            reorderCallback(source, target);
        }
        return true;
    }
    const int pressed = pressedIndex_;
    const int clickCount = pressedClickCount_;
    pressedIndex_ = -1;
    pressedClickCount_ = 1;
    resetReorderState();
    if (pressed < 0) return false;
    if (hitRowIndex(event.position) == pressed) {
        const auto previousIndices = selection_.selectedIndices();
        const int previousSelectedIndex = selectedIndex();
        if (event.button == MouseButton::Right) {
            if (!selection_.contains(pressed)) selection_.selectOnly(pressed);
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

bool Table::onMouseWheel(const MouseWheelEvent& event) {
    if (!interactive() || !contains(event.position) || maxScrollOffset() <= 0.001f) return false;
    const double nowMs = event.timestampMs > 0.0 ? event.timestampMs : currentTimeMs();
    const bool caughtUp = advanceScrollMotion(nowMs);
    const bool accepted = scrollMotion_.addDelta(
        -event.deltaY * wheelStep_, 0.0f, maxScrollOffset(), nowMs, kDefaultWheelScrollMotionSpec);
    if (accepted || scrollMotion_.running()) requestAnimationFrame();
    return accepted || caughtUp;
}

bool Table::onKeyDown(const KeyEvent& event) {
    if (!interactive() || rows_.empty()) return false;
    if (event.key == Key::A && event.control && selection_.mode() == SelectionMode::Multiple) {
        const auto previous = selection_.selectedIndices();
        const int previousIndex = selectedIndex();
        selection_.selectAll();
        notifySelectionChanged(previous, previousIndex);
        invalidate();
        return true;
    }
    const int active = selection_.activeIndex();
    if (reorderEnabled_ && event.alt && !event.control && active >= 0
        && (event.key == Key::Up || event.key == Key::Down)) {
        const int target = event.key == Key::Up
            ? std::max(0, active - 1)
            : std::min(static_cast<int>(rows_.size()) - 1, active + 1);
        const auto callback = onReorderRequested_;
        if (target != active && callback) callback(active, target);
        return true;
    }
    if (active >= 0 && event.key == Key::Enter && onActivated_) { onActivated_(active); return true; }
    if (active >= 0 && event.key == Key::F2 && onEditRequested_) { onEditRequested_(active); return true; }
    if (active >= 0 && event.key == Key::Delete && onDeleteRequested_) { onDeleteRequested_(selection_.selectedIndices()); return true; }

    int target = -1;
    if (event.key == Key::Down) target = std::min(static_cast<int>(rows_.size()) - 1, std::max(-1, active) + 1);
    else if (event.key == Key::Up) target = active < 0 ? 0 : std::max(0, active - 1);
    else if (event.key == Key::Home) target = 0;
    else if (event.key == Key::End) target = static_cast<int>(rows_.size()) - 1;
    else if (event.key == Key::PageDown || event.key == Key::PageUp) {
        const TableStyle style = resolvedStyle();
        const float contentHeight = std::max(0.0f, frame().height - style.headerHeight);
        const int visibleRows = std::max(1, static_cast<int>(std::floor(contentHeight / effectiveRowHeight(contentHeight))));
        const int origin = active < 0 ? 0 : active;
        target = event.key == Key::PageDown
            ? std::min(static_cast<int>(rows_.size()) - 1, origin + visibleRows)
            : std::max(0, origin - visibleRows);
    }
    if (target < 0) return false;
    const auto previous = selection_.selectedIndices();
    const int previousIndex = selectedIndex();
    selection_.applyKeyboardSelection(target, event.control, event.shift);
    ensureSelectionVisible();
    notifySelectionChanged(previous, previousIndex);
    invalidate();
    return true;
}

bool Table::isFocusable() const { return interactive() && !rows_.empty(); }

bool Table::tickAnimations(double nowMs) {
    const bool ticked = advanceScrollMotion(nowMs);
    if (scrollMotion_.running()) requestAnimationFrame();
    return ticked;
}

AccessibilityInfo Table::accessibilityInfo() const {
    auto info = Widget::accessibilityInfo();
    if (info.role == AccessibilityRole::None) info.role = AccessibilityRole::Table;
    info.value = std::to_wstring(rows_.size()) + L" rows, " + std::to_wstring(columns_.size()) + L" columns";
    if (selectedIndex() >= 0) info.state.selected = true;
    return info;
}

TableStyle Table::resolvedStyle() const {
    const auto& t = theme();
    TableStyle style;
    style.background = disabled() ? t.surfaceMuted : t.surface;
    style.border = t.border;
    style.headerBackground = t.surfaceMuted;
    style.headerForeground = disabled() ? t.textSubtle : t.textMuted;
    style.cellForeground = disabled() ? t.textSubtle : t.text;
    style.gridLine = t.border;
    style.rowHovered = t.surfaceMuted;
    style.rowPressed = t.pressedBackground;
    style.rowSelected = t.selectedBackground;
    style.scrollbarColor = t.textSubtle;
    style.borderWidth = 1.0f;
    style.radius = t.radiusMd;
    style.headerHeight = 32.0f;
    style.scrollbarWidth = 4.0f;
    style.cellPadding = Insets{0.0f, 10.0f};
    if (styleOverride_) applyTableStyleOverride(style, *styleOverride_);
    return style;
}

float Table::columnWidth(int index, float remainingWidth, int flexibleCount) const {
    const auto& column = columns_[static_cast<std::size_t>(index)];
    if (column.width > 0.0f) return column.width;
    return flexibleCount <= 0 ? 0.0f : remainingWidth / static_cast<float>(flexibleCount);
}

float Table::effectiveRowHeight(float contentHeight) const {
    if (rowHeight_ > 0.0f) return rowHeight_;
    return rows_.empty() ? 0.0f : contentHeight / static_cast<float>(rows_.size());
}

int Table::hitRowIndex(Point point) const {
    const TableStyle style = resolvedStyle();
    const Rect rect = frame();
    const float headerHeight = std::max(1.0f, style.headerHeight);
    const float contentHeight = std::max(0.0f, rect.height - headerHeight);
    const float height = effectiveRowHeight(contentHeight);
    if (height <= 0.0f || point.y < rect.y + headerHeight || !contains(point)) return -1;
    const int index = static_cast<int>((point.y - rect.y - headerHeight + scrollOffset_) / height);
    return index >= 0 && index < static_cast<int>(rows_.size()) ? index : -1;
}

Rect Table::rowRect(int index, float height) const {
    const TableStyle style = resolvedStyle();
    const Rect rect = frame();
    return Rect{rect.x, rect.y + style.headerHeight + static_cast<float>(index) * height - scrollOffset_, rect.width, height};
}

Rect Table::verticalThumbRect(float width, float contentHeight) const {
    const TableStyle style = resolvedStyle();
    const Rect rect = frame();
    const float row = effectiveRowHeight(contentHeight);
    const float fullHeight = static_cast<float>(rows_.size()) * row;
    const float thumbWidth = std::max(1.0f, width);
    const float thumbHeight = std::max(24.0f, contentHeight * contentHeight / std::max(contentHeight, fullHeight));
    const float travel = std::max(0.0f, contentHeight - thumbHeight - 8.0f);
    const float progress = maxScrollOffset() <= 0.001f ? 0.0f : scrollOffset_ / maxScrollOffset();
    return Rect{rect.x + rect.width - thumbWidth - 3.0f, rect.y + style.headerHeight + 4.0f + progress * travel, thumbWidth, thumbHeight};
}

void Table::ensureSelectionVisible() {
    const int selected = selection_.activeIndex();
    if (selected < 0) return;
    const TableStyle style = resolvedStyle();
    const float contentHeight = std::max(0.0f, frame().height - style.headerHeight);
    const float height = effectiveRowHeight(contentHeight);
    if (contentHeight <= 0.0f || height <= 0.0f) return;
    const float top = static_cast<float>(selected) * height;
    const float bottom = top + height;
    if (top < scrollOffset_) scrollOffset_ = top;
    else if (bottom > scrollOffset_ + contentHeight) scrollOffset_ = bottom - contentHeight;
    scrollOffset_ = std::clamp(scrollOffset_, 0.0f, maxScrollOffset());
    resetScrollMotion(scrollOffset_);
}

void Table::notifySelectionChanged(const std::vector<int>& previousIndices, int previousSelectedIndex) {
    if (previousIndices == selection_.selectedIndices()) return;
    const int next = selectedIndex();
    if (previousSelectedIndex != next && onChanged_) onChanged_(next);
    if (onSelectionChanged_) onSelectionChanged_(selection_.selectedIndices());
}

void Table::resetReorderState() {
    reordering_ = false;
    externalDragging_ = false;
    reorderStartPoint_ = {};
    reorderCurrentPoint_ = {};
    reorderSourceIndex_ = -1;
    reorderTargetIndex_ = -1;
    reorderInsertionIndex_ = -1;
}

void Table::emitItemDrag(ItemDragPhase phase, Point position) {
    if (!itemDragEnabled_ || !onItemDrag_ || reorderSourceIndex_ < 0
        || reorderSourceIndex_ >= static_cast<int>(itemDragIds_.size())) return;
    onItemDrag_(ItemDragEvent{
        itemDragIds_[static_cast<std::size_t>(reorderSourceIndex_)], phase, position});
}

void Table::updateReorderTarget(Point point) {
    const TableStyle style = resolvedStyle();
    const Rect rect = frame();
    const float contentHeight = std::max(0.0f, rect.height - style.headerHeight);
    const float height = effectiveRowHeight(contentHeight);
    reorderInsertionIndex_ = detail::reorderInsertionIndex(
        point.y,
        rect.y + style.headerHeight,
        scrollOffset_,
        height,
        static_cast<int>(rows_.size()));
    reorderTargetIndex_ = detail::reorderTargetIndex(
        reorderSourceIndex_,
        reorderInsertionIndex_,
        static_cast<int>(rows_.size()));
}

bool Table::advanceScrollMotion(double nowMs) {
    if (!scrollMotion_.running()) return false;
    const float previous = scrollOffset_;
    scrollMotion_.tick(nowMs);
    scrollOffset_ = std::clamp(scrollMotion_.value(), 0.0f, maxScrollOffset());
    if (std::fabs(scrollOffset_ - previous) > 0.001f) invalidate();
    return true;
}

void Table::resetScrollMotion(float offset) { scrollMotion_.reset(offset); }

bool Table::hasInteractionState() const {
    return hoveredIndex_ >= 0 || pressedIndex_ >= 0 || reordering_
        || !selection_.selectedIndices().empty();
}

void Table::resetInteractionState() {
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
    resetScrollMotion(scrollOffset_);
    if (notifyCancellation) callback(cancellation);
}

} // namespace oneui

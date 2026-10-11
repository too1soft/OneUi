#include "oneui/controls/table.h"

#include "oneui/icon.h"
#include "oneui/style.h"

#include "reorder_internal.h"

#include <algorithm>
#include <charconv>
#include "internal/ui_clock.h"
#include <cmath>
#if defined(__APPLE__) && defined(_LIBCPP_VERSION)
#include <locale>
#include <sstream>
#endif
#include <string>
#include <unordered_set>
#include <utility>

namespace oneui {
namespace {

double currentTimeMs() {
    return internal::uiTimeMs();
}

} // namespace

Table::Table() {
    setPreferredSize(Size{640.0f, 320.0f});
}

void Table::setColumns(std::vector<TableColumn> columns) {
    authoredColumns_ = std::move(columns);
    projectColumns();
}

void Table::setColumnPresentation(int column, TextAlign alignment, bool action) {
    if (column < 0 || column >= static_cast<int>(authoredColumns_.size())) return;
    authoredColumns_[column].alignment = alignment;
    authoredColumns_[column].action = action;
    projectColumns();
}
void Table::setColumnKind(int column, TableCellKind kind) {
    if (column < 0 || column >= static_cast<int>(authoredColumns_.size())) return;
    authoredColumns_[column].kind = kind;
    projectColumns();
}
void Table::setHeaderHeight(float height) {
    headerHeight_ = std::isfinite(height) ? std::clamp(height, 1.0f, 96.0f) : 30.0f;
    invalidate();
}
void Table::setSelectionColumnVisible(bool visible) { selectionColumnVisible_ = visible; pressedSelection_ = false; invalidate(); }
void Table::setSelectionColumnWidth(float width) {
    if (!std::isfinite(width)) return;
    selectionColumnWidth_ = std::clamp(width, 24.0f, 128.0f);
    invalidate();
}
void Table::setOnCellAction(std::function<void(int, int)> callback) { onCellAction_ = std::move(callback); }
int Table::hitColumnIndex(Point point) const {
    if (!frame().contains(point)) return -1;
    float fixed = 0.0f; int flexible = 0;
    for (const auto& column : columns_) { if (column.width > 0) fixed += column.width; else ++flexible; }
    float x = frame().x + selectionGutter();
    for (int i = 0; i < static_cast<int>(columns_.size()); ++i) {
        const float width = columnWidth(i, std::max(0.0f, frame().width - fixed - selectionGutter()), flexible);
        if (point.x >= x && point.x < x + width) return i;
        x += width;
    }
    return -1;
}

void Table::setRows(std::vector<std::vector<std::wstring>> rows) {
    std::vector<std::vector<TableCell>> richRows;
    richRows.reserve(rows.size());
    for (const auto& row : rows) {
        std::vector<TableCell> richRow;
        richRow.reserve(row.size());
        for (const auto& text : row) richRow.push_back(TableCell{text});
        richRows.push_back(std::move(richRow));
    }
    rows_ = std::move(rows);
    richRows_ = std::move(richRows);
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

void Table::setRichRows(std::vector<std::vector<TableCell>> rows) {
    std::vector<std::vector<std::wstring>> plainRows;
    plainRows.reserve(rows.size());
    for (const auto& row : rows) {
        std::vector<std::wstring> plainRow;
        plainRow.reserve(row.size());
        for (const auto& cell : row) plainRow.push_back(cell.text);
        plainRows.push_back(std::move(plainRow));
    }
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
    rows_ = std::move(plainRows);
    richRows_ = std::move(rows);
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
    rows_[index] = row;
    std::vector<TableCell> richRow;
    richRow.reserve(row.size());
    for (auto& text : row) richRow.push_back(TableCell{std::move(text)});
    richRows_[index] = std::move(richRow);
    invalidate();
    return true;
}

bool Table::updateRichRow(std::size_t index, std::vector<TableCell> row) {
    if (index >= richRows_.size()) return false;
    std::vector<std::wstring> plainRow;
    plainRow.reserve(row.size());
    for (const auto& cell : row) plainRow.push_back(cell.text);
    rows_[index] = std::move(plainRow);
    richRows_[index] = std::move(row);
    invalidate();
    return true;
}

const std::vector<TableColumn>& Table::columns() const { return columns_; }
const std::vector<std::vector<std::wstring>>& Table::rows() const { return rows_; }
const std::vector<std::vector<TableCell>>& Table::richRows() const { return richRows_; }

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

void Table::setColumnDividersVisible(bool visible) {
    if (columnDividersVisible_ == visible) return;
    columnDividersVisible_ = visible;
    invalidate();
}

bool Table::columnDividersVisible() const { return columnDividersVisible_; }

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
    const float thumbHeight = std::min(std::max(0.0f, contentHeight - 8.0f), std::max(24.0f, contentHeight * contentHeight / std::max(1.0f, fullHeight)));
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
    return draggingScrollbar_ || hoveredIndex_ >= 0 || pressedIndex_ != -1 || reordering_
        || !selection_.selectedIndices().empty();
}

void Table::resetInteractionState() {
    draggingScrollbar_ = false;
    pressedSelection_ = false;
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

#include "table_paint.inc"
#include "table_input.inc"

} // namespace oneui

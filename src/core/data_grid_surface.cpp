#include "data_grid_surface.h"

#include <algorithm>
#include <cmath>

namespace oneui {
namespace {
constexpr float kScrollbar = 10.0f;
constexpr float kResizeGrip = 4.0f;
constexpr float kSortButton = 18.0f;
constexpr float kMinColumn = 24.0f;
} // namespace

bool DataGridSelection::contains(std::int64_t row, int column) const {
    if (empty()) return false;
    switch (kind) {
    case DataGridSelectKind::All: return true;
    case DataGridSelectKind::Rows: return row >= firstRow() && row <= lastRow();
    case DataGridSelectKind::Columns: return column >= firstColumn() && column <= lastColumn();
    case DataGridSelectKind::Cells:
        return row >= firstRow() && row <= lastRow() && column >= firstColumn() && column <= lastColumn();
    }
    return false;
}

const DataGridRow* DataGridSurface::row(std::int64_t index) const {
    if (index < windowStart || index >= windowStart + static_cast<std::int64_t>(window.size())) return nullptr;
    return &window[static_cast<std::size_t>(index - windowStart)];
}

float DataGridSurface::gutterWidth() const {
    if (!rowNumbers) return 0.0f;
    int digits = 1;
    for (std::int64_t value = std::max<std::int64_t>(rowCount, 1); value >= 10; value /= 10) ++digits;
    return std::max(36.0f, digits * 7.5f + 18.0f);
}

float DataGridSurface::frozenWidth() const {
    float width = 0.0f;
    for (int i = 0; i < std::min<int>(frozenColumns, static_cast<int>(columns.size())); ++i) width += columns[i].width;
    return width;
}

float DataGridSurface::totalWidth() const {
    float width = gutterWidth();
    for (const auto& column : columns) width += column.width;
    return width;
}

Rect DataGridSurface::bodyRect() const {
    const Rect f = frame();
    const bool vertical = rowCount * static_cast<double>(rowHeight) > f.height - headerHeight;
    const bool horizontal = totalWidth() > f.width - (vertical ? kScrollbar : 0.0f);
    return Rect{f.x, f.y + headerHeight, std::max(0.0f, f.width - (vertical ? kScrollbar : 0.0f)),
                std::max(0.0f, f.height - headerHeight - (horizontal ? kScrollbar : 0.0f))};
}

double DataGridSurface::maxScroll() const {
    return std::max(0.0, rowCount * static_cast<double>(rowHeight) - bodyRect().height);
}

float DataGridSurface::maxHorizontal() const { return std::max(0.0f, totalWidth() - bodyRect().width); }

float DataGridSurface::columnLeft(int column) const {
    float x = gutterWidth();
    for (int i = 0; i < column && i < static_cast<int>(columns.size()); ++i) x += columns[i].width;
    return x;
}

float DataGridSurface::columnScreenX(int column) const {
    const float left = frame().x + columnLeft(column);
    return column < frozenColumns ? left : left - horizontal_;
}

std::int64_t DataGridSurface::firstVisible() const {
    return rowHeight > 0 ? static_cast<std::int64_t>(std::floor(scroll_ / rowHeight)) : 0;
}

std::int64_t DataGridSurface::visibleCount() const {
    return rowHeight > 0 ? static_cast<std::int64_t>(std::ceil(bodyRect().height / rowHeight)) + 1 : 0;
}

std::optional<Rect> DataGridSurface::cellRect(std::int64_t row, int column) const {
    if (row < 0 || row >= rowCount || column < 0 || column >= static_cast<int>(columns.size())) return {};
    const Rect body = bodyRect();
    const float y = static_cast<float>(body.y + row * static_cast<double>(rowHeight) - scroll_);
    return Rect{columnScreenX(column), y, columns[column].width, rowHeight};
}

void DataGridSurface::setScroll(double value) {
    const double next = std::clamp(value, 0.0, maxScroll());
    if (next == scroll_) return;
    scroll_ = next;
    requestWindow();
    owner_.layoutChildren();
    invalidate();
}

void DataGridSurface::setHorizontal(float value) {
    const float next = std::clamp(value, 0.0f, maxHorizontal());
    if (next == horizontal_) return;
    horizontal_ = next;
    owner_.layoutChildren();
    invalidate();
}

void DataGridSurface::scrollTo(std::int64_t row, int column) {
    const Rect body = bodyRect();
    if (row >= 0 && rowHeight > 0) {
        const double top = row * static_cast<double>(rowHeight);
        if (top < scroll_) setScroll(top);
        else if (top + rowHeight > scroll_ + body.height) setScroll(top + rowHeight - body.height);
    }
    if (column >= frozenColumns && column < static_cast<int>(columns.size())) {
        const float left = columnLeft(column) - gutterWidth() - frozenWidth();
        const float visible = body.width - gutterWidth() - frozenWidth();
        if (left < horizontal_) setHorizontal(left);
        else if (left + columns[column].width > horizontal_ + visible) setHorizontal(left + columns[column].width - visible);
    }
}

void DataGridSurface::requestWindow() {
    if (!onViewport || rowCount <= 0) return;
    const std::int64_t first = std::min(firstVisible(), rowCount - 1);
    const std::int64_t count = std::min(visibleCount(), rowCount - first);
    const bool covered = requestedFirst_ >= 0 && first >= requestedFirst_ &&
                         first + count <= requestedFirst_ + requestedCount_;
    if (covered) return;
    // Ask for a screen above and below so ordinary scrolling stays inside the window.
    const std::int64_t start = std::max<std::int64_t>(0, first - count);
    const std::int64_t total = std::min(rowCount - start, count * 3);
    requestedFirst_ = start;
    requestedCount_ = total;
    onViewport(start, total);
}

void DataGridSurface::dataChanged() {
    // A shrunken source invalidates what was asked for.
    if (requestedFirst_ + requestedCount_ > rowCount) requestedFirst_ = -1;
    scroll_ = std::clamp(scroll_, 0.0, maxScroll());
    horizontal_ = std::clamp(horizontal_, 0.0f, maxHorizontal());
    if (!selection.empty() && (selection.focusRow >= rowCount || selection.focusColumn >= static_cast<int>(columns.size()))) {
        DataGridSelection none;
        setSelection(none, true);
    }
    requestWindow();
    invalidate();
}

void DataGridSurface::sizeChanged() {
    scroll_ = std::clamp(scroll_, 0.0, maxScroll());
    horizontal_ = std::clamp(horizontal_, 0.0f, maxHorizontal());
    requestWindow();
}

void DataGridSurface::setSelection(const DataGridSelection& next, bool notify) {
    if (next == selection) return;
    selection = next;
    invalidate();
    if (notify && onSelectionChanged) onSelectionChanged(selection);
}

void DataGridSurface::move(std::int64_t rows, int columnsBy, bool extend) {
    if (rowCount <= 0 || columns.empty()) return;
    DataGridSelection next = selection;
    if (next.empty()) {
        next = DataGridSelection{0, 0, 0, 0, DataGridSelectKind::Cells};
    } else {
        next.focusRow = std::clamp<std::int64_t>(next.focusRow + rows, 0, rowCount - 1);
        next.focusColumn = std::clamp(next.focusColumn + columnsBy, 0, static_cast<int>(columns.size()) - 1);
        if (!extend) {
            next.anchorRow = next.focusRow;
            next.anchorColumn = next.focusColumn;
        }
        next.kind = DataGridSelectKind::Cells;
    }
    setSelection(next, true);
    scrollTo(next.focusRow, next.focusColumn);
}

DataGridSurface::Hit DataGridSurface::hit(Point point) const {
    Hit h;
    const Rect f = frame();
    const Rect body = bodyRect();
    if (point.x >= body.x + body.width && point.y >= body.y && point.y < body.y + body.height) {
        h.verticalBar = true;
        return h;
    }
    if (point.y >= body.y + body.height && point.x < body.x + body.width) {
        h.horizontalBar = true;
        return h;
    }
    h.header = point.y < f.y + headerHeight;
    h.gutter = point.x < f.x + gutterWidth();
    if (!h.gutter) {
        for (int column = 0; column < static_cast<int>(columns.size()); ++column) {
            const float left = columnScreenX(column);
            const float right = left + columns[column].width;
            // Frozen columns cover scrolled ones underneath them.
            if (column >= frozenColumns && left < f.x + gutterWidth() + frozenWidth()) continue;
            if (h.header && std::abs(point.x - right) <= kResizeGrip) h.resizeColumn = column;
            if (point.x >= left && point.x < right) {
                h.column = column;
                h.sortButton = h.header && point.x >= right - kSortButton && h.resizeColumn < 0;
            }
        }
    }
    if (!h.header && rowHeight > 0) {
        const double offset = point.y - body.y + scroll_;
        const auto row = static_cast<std::int64_t>(std::floor(offset / rowHeight));
        if (row >= 0 && row < rowCount) h.row = row;
    }
    return h;
}

CursorKind DataGridSurface::cursor(Point point) const {
    if (drag_ == Drag::Resize || hit(point).resizeColumn >= 0) return CursorKind::ResizeHorizontal;
    return CursorKind::Default;
}

bool DataGridSurface::onMouseDown(const MouseEvent& event) {
    if (!contains(event.position)) return false;
    const Hit h = hit(event.position);
    if (h.verticalBar || h.horizontalBar) {
        const Rect thumb = h.verticalBar ? verticalThumb() : horizontalThumb();
        if (thumb.contains(event.position)) {
            drag_ = h.verticalBar ? Drag::VerticalThumb : Drag::HorizontalThumb;
            thumbGrab_ = h.verticalBar ? event.position.y - thumb.y : event.position.x - thumb.x;
        } else if (h.verticalBar) {
            setScroll(scroll_ + (event.position.y < thumb.y ? -1 : 1) * bodyRect().height);
        } else {
            setHorizontal(horizontal_ + (event.position.x < thumb.x ? -1 : 1) * bodyRect().width);
        }
        return true;
    }
    if (h.header) {
        if (h.resizeColumn >= 0) {
            if (event.clickCount >= 2) {
                autofitColumn_ = h.resizeColumn;
                invalidate();
            } else {
                drag_ = Drag::Resize;
                resizeColumn_ = h.resizeColumn;
                resizeOrigin_ = event.position.x;
                resizeStartWidth_ = columns[h.resizeColumn].width;
            }
            return true;
        }
        if (h.gutter) {
            if (rowCount > 0 && !columns.empty())
                setSelection({0, 0, rowCount - 1, static_cast<int>(columns.size()) - 1, DataGridSelectKind::All}, true);
            return true;
        }
        if (h.column < 0) return true;
        if (h.sortButton) {
            if (onSort) onSort(h.column);
            return true;
        }
        DataGridSelection next = selection;
        const int anchor = event.shift && !selection.empty() ? selection.anchorColumn : h.column;
        next = {0, anchor, std::max<std::int64_t>(0, rowCount - 1), h.column, DataGridSelectKind::Columns};
        if (rowCount > 0) setSelection(next, true);
        drag_ = Drag::Columns;
        return true;
    }
    if (h.row < 0) return true;
    if (h.gutter) {
        const std::int64_t anchor = event.shift && !selection.empty() ? selection.anchorRow : h.row;
        setSelection({anchor, 0, h.row, std::max(0, static_cast<int>(columns.size()) - 1), DataGridSelectKind::Rows}, true);
        drag_ = Drag::Rows;
        return true;
    }
    if (h.column < 0) return true;
    if (event.button == MouseButton::Right) {
        if (!selection.contains(h.row, h.column))
            setSelection({h.row, h.column, h.row, h.column, DataGridSelectKind::Cells}, true);
        if (onContextMenu) onContextMenu(h.row, h.column, event.position);
        return true;
    }
    if (event.clickCount >= 2) {
        if (editable) owner_.beginEdit();
        else if (onActivate) onActivate(h.row, h.column);
        return true;
    }
    if (event.shift && !selection.empty()) {
        DataGridSelection next = selection;
        next.focusRow = h.row;
        next.focusColumn = h.column;
        next.kind = DataGridSelectKind::Cells;
        setSelection(next, true);
    } else {
        setSelection({h.row, h.column, h.row, h.column, DataGridSelectKind::Cells}, true);
    }
    drag_ = Drag::Cells;
    return true;
}

bool DataGridSurface::onMouseMove(const MouseEvent& event) {
    switch (drag_) {
    case Drag::None: return false;
    case Drag::Resize: {
        auto& column = columns[resizeColumn_];
        column.width = std::max(kMinColumn, resizeStartWidth_ + event.position.x - resizeOrigin_);
        horizontal_ = std::clamp(horizontal_, 0.0f, maxHorizontal());
        owner_.layoutChildren();
        invalidate();
        return true;
    }
    case Drag::VerticalThumb: {
        const Rect body = bodyRect();
        const Rect thumb = verticalThumb();
        const float track = std::max(1.0f, body.height - thumb.height);
        setScroll((event.position.y - thumbGrab_ - body.y) / track * maxScroll());
        return true;
    }
    case Drag::HorizontalThumb: {
        const Rect body = bodyRect();
        const Rect thumb = horizontalThumb();
        const float track = std::max(1.0f, body.width - thumb.width);
        setHorizontal((event.position.x - thumbGrab_ - body.x) / track * maxHorizontal());
        return true;
    }
    case Drag::Cells:
    case Drag::Rows:
    case Drag::Columns: {
        const Rect body = bodyRect();
        // Dragging past an edge scrolls toward it.
        if (event.position.y < body.y) setScroll(scroll_ - rowHeight);
        else if (event.position.y > body.y + body.height) setScroll(scroll_ + rowHeight);
        Point clamped{std::clamp(event.position.x, body.x + gutterWidth() + 1.0f, body.x + body.width - 1.0f),
                      std::clamp(event.position.y, body.y + 1.0f, body.y + body.height - 1.0f)};
        const Hit h = hit(clamped);
        DataGridSelection next = selection;
        if (drag_ != Drag::Columns && h.row >= 0) next.focusRow = h.row;
        if (drag_ != Drag::Rows && h.column >= 0) next.focusColumn = h.column;
        setSelection(next, true);
        return true;
    }
    }
    return false;
}

bool DataGridSurface::onMouseUp(const MouseEvent&) {
    if (drag_ == Drag::None) return false;
    if (drag_ == Drag::Resize && onResize) onResize(resizeColumn_, columns[resizeColumn_].width);
    drag_ = Drag::None;
    resizeColumn_ = -1;
    return true;
}

bool DataGridSurface::onMouseWheel(const MouseWheelEvent& event) {
    if (!contains(event.position)) return false;
    const float step = rowHeight * 3.0f;
    if (event.shift) setHorizontal(horizontal_ - event.deltaY * step * 2.0f);
    else setScroll(scroll_ - event.deltaY * step);
    return true;
}

bool DataGridSurface::onKeyDown(const KeyEvent& event) {
    if (rowCount <= 0 || columns.empty()) return false;
    const auto page = std::max<std::int64_t>(1, visibleCount() - 2);
    const auto last = static_cast<int>(columns.size()) - 1;
    const auto focusRow = selection.empty() ? 0 : selection.focusRow;
    const auto focusColumn = selection.empty() ? 0 : selection.focusColumn;
    switch (event.key) {
    case Key::Up: move(event.control ? -focusRow : -1, 0, event.shift); return true;
    case Key::Down: move(event.control ? rowCount : 1, 0, event.shift); return true;
    case Key::Left: move(0, event.control ? -focusColumn : -1, event.shift); return true;
    case Key::Right: move(0, event.control ? last : 1, event.shift); return true;
    case Key::PageUp: move(-page, 0, event.shift); return true;
    case Key::PageDown: move(page, 0, event.shift); return true;
    case Key::Home: move(event.control ? -focusRow : 0, -focusColumn, event.shift); return true;
    case Key::End: move(event.control ? rowCount : 0, last, event.shift); return true;
    case Key::Tab: move(0, event.shift ? -1 : 1, false); return true;
    case Key::A:
        if (!event.editShortcut()) return false;
        setSelection({0, 0, rowCount - 1, last, DataGridSelectKind::All}, true);
        return true;
    case Key::C:
        if (!event.editShortcut()) return false;
        if (onCopy) onCopy();
        return true;
    case Key::Enter:
        if (selection.empty()) return false;
        if (editable) return owner_.beginEdit();
        if (onActivate) onActivate(selection.focusRow, selection.focusColumn);
        return true;
    case Key::F2: return editable && owner_.beginEdit();
    default: break;
    }
    // Keys without a logical Key value arrive as Win32-compatible virtual keys.
    constexpr unsigned insertKey = 0x2d, appsKey = 0x5d, f10Key = 0x79;
    if (event.virtualKey == insertKey && event.control && !event.shift && !event.alt) {
        if (onCopy) onCopy();
        return true;
    }
    if (event.virtualKey == appsKey || (event.virtualKey == f10Key && event.shift)) {
        if (selection.empty() || !onContextMenu) return false;
        const auto rect = cellRect(selection.focusRow, selection.focusColumn);
        const Rect body = bodyRect();
        const Point at = rect ? Point{rect->x + 4.0f, rect->y + rect->height} : Point{body.x + 4.0f, body.y};
        onContextMenu(selection.focusRow, selection.focusColumn, at);
        return true;
    }
    return false;
}

Rect DataGridSurface::verticalThumb() const {
    const Rect body = bodyRect();
    const double content = rowCount * static_cast<double>(rowHeight);
    if (content <= body.height) return {};
    const float height = std::max(24.0f, static_cast<float>(body.height * body.height / content));
    const float y = body.y + static_cast<float>((body.height - height) * (scroll_ / std::max(1.0, maxScroll())));
    return Rect{body.x + body.width, y, kScrollbar, height};
}

Rect DataGridSurface::horizontalThumb() const {
    const Rect body = bodyRect();
    const float content = totalWidth();
    if (content <= body.width) return {};
    const float width = std::max(24.0f, body.width * body.width / content);
    const float x = body.x + (body.width - width) * (horizontal_ / std::max(1.0f, maxHorizontal()));
    return Rect{x, body.y + body.height, width, kScrollbar};
}

} // namespace oneui

#include "oneui/layout/grid.h"

#include <algorithm>

namespace oneui {

Grid::Grid(int columns) {
    setColumns(columns);
}

void Grid::setColumns(int columns) {
    columns_ = std::max(1, columns);
    invalidate();
}

void Grid::setGap(float gap) {
    columnGap_ = gap;
    rowGap_ = gap;
    invalidate();
}

void Grid::setColumnGap(float gap) {
    columnGap_ = gap;
    invalidate();
}

void Grid::setRowGap(float gap) {
    rowGap_ = gap;
    invalidate();
}

void Grid::setPadding(Insets padding) {
    padding_ = padding;
    invalidate();
}

void Grid::setAutoRows(float height) {
    autoRows_ = height;
    invalidate();
}

void Grid::layoutChildren() {
    const Rect content = frame().inset(padding_);
    const int columns = std::max(1, columns_);
    const float totalGap = columnGap_ * static_cast<float>(columns - 1);
    const float cellWidth = std::max(0.0f, (content.width - totalGap) / static_cast<float>(columns));

    int index = 0;
    for (const auto& child : children()) {
        if (!child->visible()) {
            continue;
        }

        const int row = index / columns;
        const int column = index % columns;
        const Size preferred = child->preferredSize();
        const float cellHeight = autoRows_ > 0.0f ? autoRows_ : std::max(0.0f, preferred.height);
        const float width = preferred.width > 0.0f ? std::min(preferred.width, cellWidth) : cellWidth;
        const float x = content.x + static_cast<float>(column) * (cellWidth + columnGap_);
        const float y = content.y + static_cast<float>(row) * (cellHeight + rowGap_);

        child->setFrame(Rect{x, y, width, cellHeight});
        ++index;
    }
}

} // namespace oneui

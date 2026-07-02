#include "oneui/layout/wrap.h"

#include <algorithm>

namespace oneui {

void Wrap::setGap(float gap) {
    gap_ = gap;
    invalidate();
}

void Wrap::setRowGap(float gap) {
    rowGap_ = gap;
    invalidate();
}

void Wrap::setPadding(Insets padding) {
    padding_ = padding;
    invalidate();
}

void Wrap::layoutChildren() {
    const Rect content = frame().inset(padding_);
    const float maxRight = content.x + std::max(0.0f, content.width);
    float cursorX = content.x;
    float cursorY = content.y;
    float rowHeight = 0.0f;
    bool rowHasChild = false;

    for (const auto& child : children()) {
        if (!child->visible()) {
            continue;
        }

        const Size preferred = child->preferredSize();
        const float width = std::max(0.0f, preferred.width);
        const float height = std::max(0.0f, preferred.height);
        const bool overflowsRow = rowHasChild && cursorX + width > maxRight;

        if (overflowsRow) {
            cursorX = content.x;
            cursorY += rowHeight + rowGap_;
            rowHeight = 0.0f;
            rowHasChild = false;
        }

        child->setFrame(Rect{cursorX, cursorY, width, height});
        cursorX += width + gap_;
        rowHeight = std::max(rowHeight, height);
        rowHasChild = true;
    }
}

} // namespace oneui

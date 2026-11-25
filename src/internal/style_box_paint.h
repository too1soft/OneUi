#pragma once

namespace oneui {
struct StyleBox;

namespace internal {
// Compares the inputs shared by paintStyleBox and stylePaintBounds.
bool sameStyleBoxPaint(const StyleBox& left, const StyleBox& right);
}
}

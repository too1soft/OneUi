#include "oneui/canvas.h"
#include "text/text_layout.h"

#include <cmath>

namespace oneui {
std::wstring Canvas::ellipsizeText(const std::wstring& value, float maxWidth, float size,
                                  int weight, TextFontFamily family) const {
    if (value.empty() || !std::isfinite(maxWidth) || maxWidth <= 0) return {};
    if (measureTextWidthWithFont(value, size, family, weight) <= maxWidth) return value;
    const std::wstring ellipsis = L"\u2026";
    if (measureTextWidthWithFont(ellipsis, size, family, weight) > maxWidth) return {};
    text::LayoutOptions options;
    options.size = size; options.weight = weight; options.fallbackFamily = family;
    options.family = defaultFontFamily();
    const auto layout = text::Layout::make(value, options);
    const auto& boundaries = layout->graphemes();
    std::size_t low = 0, high = boundaries.size() - 1;
    while (low < high) {
        const auto middle = low + (high - low + 1) / 2;
        if (measureTextWidthWithFont(value.substr(0, boundaries[middle]) + ellipsis, size, family, weight) <= maxWidth)
            low = middle;
        else high = middle - 1;
    }
    return value.substr(0, boundaries[low]) + ellipsis;
}
} // namespace oneui

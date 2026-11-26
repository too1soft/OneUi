#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace oneui::text {
bool warmRegisteredGlyphMetrics(std::string_view text, std::string_view family, float size, int weight);
bool warmRegisteredGlyphMasks(std::string_view text, std::string_view family, float size, int weight,
                              float scale, const uint32_t* colors, std::size_t colorCount);
}

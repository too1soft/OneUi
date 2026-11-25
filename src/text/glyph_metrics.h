#pragma once

#include <string_view>

namespace oneui::text {
bool warmRegisteredGlyphMetrics(std::string_view text, std::string_view family, float size, int weight);
}

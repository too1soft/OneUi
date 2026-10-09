#pragma once

#include <cstddef>
#include <string>

namespace oneui {

enum class TextDirection : unsigned int { Auto = 0, LTR = 1, RTL = 2 };
enum class TextWrapMode : unsigned int { NoWrap = 0, WordWrap = 1 };
enum class TextAffinity : unsigned int { Upstream = 0, Downstream = 1 };

// New portable positions use UTF-8 byte offsets, never glyph or wchar_t indices.
struct TextPosition {
    std::size_t utf8Offset = 0;
    TextAffinity affinity = TextAffinity::Downstream;
};

struct TextOptions {
    TextDirection direction = TextDirection::Auto;
    TextWrapMode wrap = TextWrapMode::NoWrap;
    std::string locale = "und";
    // Logical pixels, applied by shaping (including measurement and hit testing).
    float letterSpacing = 0.0f;
    bool tabularNumbers = false;
};

} // namespace oneui

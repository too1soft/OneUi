#pragma once

#include "internal/unicode.h"

#include <limits>
#include <stdexcept>
#include <vector>

namespace oneui::text::detail {

// SkParagraph mixes UTF-8 byte offsets with UTF-16 selection ranges. Decode
// once so the two maps agree on surrogate pairs and invalid wide input.
struct TextIndex {
    std::string utf8;
    std::u16string utf16;
    std::vector<std::size_t> wideTo8, wideTo16, utf8ToWide;

    explicit TextIndex(std::wstring_view value) {
        utf8.reserve(value.size());
        utf16.reserve(value.size());
        wideTo8.resize(value.size() + 1);
        wideTo16.resize(value.size() + 1);
        utf8ToWide.reserve(value.size() + 1);
        for (std::size_t start = 0; start < value.size();) {
            const auto end = unicode::next(value, start);
            auto codePoint = static_cast<std::uint32_t>(value[start]);
            if (end - start == 2) {
                const auto low = static_cast<std::uint32_t>(value[start + 1]);
                codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (low - 0xDC00);
            }
            if (!unicode::isUnicodeScalar(codePoint)) codePoint = unicode::kReplacementCodePoint;
            for (auto unit = start; unit < end; ++unit) {
                wideTo8[unit] = utf8.size();
                wideTo16[unit] = utf16.size();
            }
            unicode::appendUtf8CodePoint(utf8, codePoint);
            utf8ToWide.resize(utf8.size(), start);
            if (codePoint > 0xFFFF) {
                utf16.push_back(static_cast<char16_t>(0xD800 + ((codePoint - 0x10000) >> 10)));
                utf16.push_back(static_cast<char16_t>(0xDC00 + ((codePoint - 0x10000) & 0x3FF)));
            } else {
                utf16.push_back(static_cast<char16_t>(codePoint));
            }
            start = end;
        }
        if (utf8.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            throw std::length_error("OneUI paragraph exceeds the text engine's index range");
        wideTo8.back() = utf8.size();
        wideTo16.back() = utf16.size();
        utf8ToWide.push_back(value.size());
    }
};

} // namespace oneui::text::detail

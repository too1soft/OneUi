#pragma once
#include "internal/unicode.h"
#include <utility>

namespace oneui::platform {
inline std::size_t utf8BoundaryBefore(std::string_view text, std::size_t at) {
    at = std::min(at, text.size());
    while (at && at < text.size() && (static_cast<unsigned char>(text[at]) & 0xC0) == 0x80) --at;
    return at;
}
inline std::size_t utf8BoundaryAfter(std::string_view text, std::size_t at) {
    at = std::min(at, text.size());
    while (at < text.size() && (static_cast<unsigned char>(text[at]) & 0xC0) == 0x80) ++at;
    return at;
}
// Protocol lengths are UTF-8 bytes outside the selection, not wchar_t units.
inline std::pair<std::size_t, std::size_t> surroundingDeletionRange(
    std::wstring_view text, std::size_t anchor, std::size_t caret,
    std::size_t beforeBytes, std::size_t afterBytes) {
    const auto left = unicode::boundary(text, std::min(anchor, caret));
    const auto right = unicode::boundary(text, std::max(anchor, caret));
    const auto prefix = unicode::toUtf8(text.substr(0, left));
    const auto suffix = unicode::toUtf8(text.substr(right));
    const auto start = utf8BoundaryBefore(prefix, prefix.size() - std::min(beforeBytes, prefix.size()));
    const auto end = utf8BoundaryAfter(suffix, afterBytes);
    return {unicode::fromUtf8(std::string_view(prefix).substr(0, start)).size(),
            right + unicode::fromUtf8(std::string_view(suffix).substr(0, end)).size()};
}
struct SurroundingText {
    std::string text;
    std::size_t anchor = 0, caret = 0;
};
inline SurroundingText surroundingText(std::wstring_view text, std::size_t anchor, std::size_t caret,
                                      std::size_t limit = 4000) {
    auto bytes = unicode::toUtf8(text);
    anchor = unicode::toUtf8(text.substr(0, unicode::boundary(text, anchor))).size();
    caret = unicode::toUtf8(text.substr(0, unicode::boundary(text, caret))).size();
    if (bytes.size() <= limit) return {std::move(bytes), anchor, caret};
    const auto left = std::min(anchor, caret), right = std::max(anchor, caret);
    if (right - left > limit) return {}; // No valid protocol window covers the selection.
    auto start = left - std::min(left, (limit - (right - left)) / 2);
    auto end = std::min(bytes.size(), start + limit);
    start = utf8BoundaryAfter(bytes, end > limit ? end - limit : 0);
    end = utf8BoundaryBefore(bytes, end);
    return {bytes.substr(start, end - start), anchor - start, caret - start};
}
} // namespace oneui::platform

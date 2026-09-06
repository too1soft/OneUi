#pragma once
#include <algorithm>
#include <climits>
#include <cstdint>
#include <string>
#include <string_view>

namespace oneui::unicode {
constexpr std::uint32_t kReplacementCodePoint = 0xFFFD;

inline bool isUnicodeScalar(std::uint32_t codePoint) {
    return codePoint <= 0x10FFFF && !(codePoint >= 0xD800 && codePoint <= 0xDFFF);
}

inline void appendWideCodePoint(std::wstring& target, std::uint32_t codePoint) {
    if (!isUnicodeScalar(codePoint)) {
        codePoint = kReplacementCodePoint;
    }
#if WCHAR_MAX <= 0xFFFF
    if (codePoint <= 0xFFFF) {
        target.push_back(static_cast<wchar_t>(codePoint));
        return;
    }
    codePoint -= 0x10000;
    target.push_back(static_cast<wchar_t>(0xD800 + (codePoint >> 10)));
    target.push_back(static_cast<wchar_t>(0xDC00 + (codePoint & 0x3FF)));
#else
    target.push_back(static_cast<wchar_t>(codePoint));
#endif
}

inline void appendUtf8CodePoint(std::string& target, std::uint32_t codePoint) {
    if (!isUnicodeScalar(codePoint)) {
        codePoint = kReplacementCodePoint;
    }
    if (codePoint <= 0x7F) {
        target.push_back(static_cast<char>(codePoint));
    } else if (codePoint <= 0x7FF) {
        target.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
        target.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else if (codePoint <= 0xFFFF) {
        target.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
        target.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        target.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else {
        target.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
        target.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
        target.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        target.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
}

inline std::wstring fromUtf8(std::string_view text) {
    if (text.empty()) {
        return {};
    }

    std::wstring result;
    result.reserve(text.size());
    const auto* bytes = reinterpret_cast<const unsigned char*>(text.data());
    for (std::size_t index = 0; index < text.size();) {
        const unsigned char first = bytes[index];
        std::uint32_t codePoint = 0;
        std::size_t sequenceLength = 1;
        std::uint32_t minimumCodePoint = 0;
        if (first <= 0x7F) {
            codePoint = first;
        } else if ((first & 0xE0) == 0xC0) {
            codePoint = first & 0x1F;
            sequenceLength = 2;
            minimumCodePoint = 0x80;
        } else if ((first & 0xF0) == 0xE0) {
            codePoint = first & 0x0F;
            sequenceLength = 3;
            minimumCodePoint = 0x800;
        } else if ((first & 0xF8) == 0xF0) {
            codePoint = first & 0x07;
            sequenceLength = 4;
            minimumCodePoint = 0x10000;
        } else {
            appendWideCodePoint(result, kReplacementCodePoint);
            ++index;
            continue;
        }

        bool valid = index + sequenceLength <= text.size();
        for (std::size_t offset = 1; valid && offset < sequenceLength; ++offset) {
            const unsigned char next = bytes[index + offset];
            if ((next & 0xC0) != 0x80) {
                valid = false;
                break;
            }
            codePoint = (codePoint << 6) | (next & 0x3F);
        }
        if (!valid || codePoint < minimumCodePoint || !isUnicodeScalar(codePoint)) {
            appendWideCodePoint(result, kReplacementCodePoint);
            ++index;
            continue;
        }

        appendWideCodePoint(result, codePoint);
        index += sequenceLength;
    }
    return result;
}

inline std::string toUtf8(std::wstring_view text) {
    std::string result;
    result.reserve(text.size());
    for (std::size_t index = 0; index < text.size(); ++index) {
        std::uint32_t codePoint = static_cast<std::uint32_t>(text[index]);
#if WCHAR_MAX <= 0xFFFF
        if (codePoint >= 0xD800 && codePoint <= 0xDBFF) {
            if (index + 1 < text.size()) {
                const std::uint32_t low = static_cast<std::uint32_t>(text[index + 1]);
                if (low >= 0xDC00 && low <= 0xDFFF) {
                    codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (low - 0xDC00);
                    ++index;
                } else {
                    codePoint = kReplacementCodePoint;
                }
            } else {
                codePoint = kReplacementCodePoint;
            }
        } else if (codePoint >= 0xDC00 && codePoint <= 0xDFFF) {
            codePoint = kReplacementCodePoint;
        }
#endif
        appendUtf8CodePoint(result, codePoint);
    }
    return result;
}


inline std::size_t boundary(std::wstring_view text, std::size_t offset) {
    offset = std::min(offset, text.size());
    if constexpr (sizeof(wchar_t) == 2) {
        if (offset > 0 && offset < text.size() &&
            text[offset] >= 0xDC00 && text[offset] <= 0xDFFF &&
            text[offset - 1] >= 0xD800 && text[offset - 1] <= 0xDBFF) --offset;
    }
    return offset;
}
inline std::size_t previous(std::wstring_view text, std::size_t offset) {
    return boundary(text, offset > 0 ? std::min(offset, text.size()) - 1 : 0);
}
inline std::size_t next(std::wstring_view text, std::size_t offset) {
    offset = boundary(text, offset);
    if (offset == text.size()) return offset;
    if constexpr (sizeof(wchar_t) == 2) {
        if (offset + 1 < text.size() && text[offset] >= 0xD800 && text[offset] <= 0xDBFF &&
            text[offset + 1] >= 0xDC00 && text[offset + 1] <= 0xDFFF) return offset + 2;
    }
    return offset + 1;
}
} // namespace oneui::unicode

#include "oneui_c_api_internal.h"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <sstream>

namespace oneui::capi {
namespace {

constexpr std::uint32_t kReplacementCodePoint = 0xFFFD;

bool isUnicodeScalar(std::uint32_t codePoint) {
    return codePoint <= 0x10FFFF && !(codePoint >= 0xD800 && codePoint <= 0xDFFF);
}

void appendWideCodePoint(std::wstring& target, std::uint32_t codePoint) {
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

void appendUtf8CodePoint(std::string& target, std::uint32_t codePoint) {
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

std::vector<std::wstring> splitWideBy(const std::wstring& text, wchar_t delimiter) {
    std::vector<std::wstring> result;
    std::wstringstream stream(text);
    std::wstring part;
    while (std::getline(stream, part, delimiter)) {
        result.push_back(part);
    }
    return result;
}

} // namespace

std::wstring wideOrEmpty(const wchar_t* text) {
    return text ? std::wstring(text) : std::wstring();
}

std::wstring utf8OrEmpty(OneUiUtf8String text) {
    if (!text.data || text.length == 0) {
        return {};
    }

    std::wstring result;
    result.reserve(text.length);
    const auto* bytes = reinterpret_cast<const unsigned char*>(text.data);
    for (std::size_t index = 0; index < text.length;) {
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

        bool valid = index + sequenceLength <= text.length;
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

std::string utf8FromWide(const std::wstring& text) {
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

Insets toNativeInsets(OneUiInsets insets) {
    return Insets{insets.top, insets.right, insets.bottom, insets.left};
}

Color toNativeColor(OneUiColor color) {
    return Color{color.r, color.g, color.b, color.a};
}

std::vector<std::wstring> splitWideItems(const wchar_t* text) {
    std::vector<std::wstring> result;
    if (!text) {
        return result;
    }
    std::wstringstream stream(text);
    std::wstring part;
    while (std::getline(stream, part, L'|')) {
        if (!part.empty()) {
            result.push_back(part);
        }
    }
    return result;
}

std::vector<ListItem> splitWideListItems(const wchar_t* text) {
    std::vector<ListItem> result;
    for (const auto& row : splitWideItems(text)) {
        const auto parts = splitWideBy(row, L'\t');
        ListItem item;
        if (!parts.empty()) {
            item.title = parts[0];
        }
        if (parts.size() > 1) {
            item.detail = parts[1];
        }
        if (!item.title.empty() || !item.detail.empty()) {
            result.push_back(std::move(item));
        }
    }
    return result;
}

std::vector<ListItem> listItemsFromUtf8(const OneUiListItemUtf8* items, std::size_t count) {
    std::vector<ListItem> result;
    if (!items || count == 0) {
        return result;
    }
    result.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        ListItem item{utf8OrEmpty(items[index].title), utf8OrEmpty(items[index].detail)};
        if (!item.title.empty() || !item.detail.empty()) {
            result.push_back(std::move(item));
        }
    }
    return result;
}

VirtualListItem richListItemFromUtf8(const OneUiRichListItemUtf8& item) {
    return VirtualListItem{
        utf8OrEmpty(item.title),
        utf8OrEmpty(item.detail),
        utf8OrEmpty(item.badge),
        utf8OrEmpty(item.trailing),
        toNativeColor(item.indicator_color),
        toNativeColor(item.trailing_color),
        item.indicator_visible != 0};
}

std::vector<VirtualListItem> richListItemsFromUtf8(
    const OneUiRichListItemUtf8* items,
    std::size_t count) {
    std::vector<VirtualListItem> result;
    if (!items || count == 0) {
        return result;
    }
    result.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        auto item = richListItemFromUtf8(items[index]);
        if (!item.title.empty() || !item.detail.empty() || !item.badge.empty() || !item.trailing.empty()) {
            result.push_back(std::move(item));
        }
    }
    return result;
}

} // namespace oneui::capi

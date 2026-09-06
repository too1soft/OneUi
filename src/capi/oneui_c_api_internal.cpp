#include "oneui_c_api_internal.h"
#include "../internal/unicode.h"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <sstream>

namespace oneui::capi {
namespace {

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
    return text.data ? unicode::fromUtf8(std::string_view(text.data, text.length)) : std::wstring{};
}

std::string utf8FromWide(const std::wstring& text) { return unicode::toUtf8(text); }

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

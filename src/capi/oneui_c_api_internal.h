#pragma once

#include "oneui/controls/list.h"
#include "oneui/controls/virtual_list.h"
#include "oneui/oneui_c_api.h"

#include <string>
#include <vector>

namespace oneui::capi {

std::wstring wideOrEmpty(const wchar_t* text);
std::wstring utf8OrEmpty(OneUiUtf8String text);
std::string utf8FromWide(const std::wstring& text);
Insets toNativeInsets(OneUiInsets insets);
Color toNativeColor(OneUiColor color);

std::vector<std::wstring> splitWideItems(const wchar_t* text);
std::vector<ListItem> splitWideListItems(const wchar_t* text);
std::vector<ListItem> listItemsFromUtf8(const OneUiListItemUtf8* items, std::size_t count);
VirtualListItem richListItemFromUtf8(const OneUiRichListItemUtf8& item);
std::vector<VirtualListItem> richListItemsFromUtf8(
    const OneUiRichListItemUtf8* items,
    std::size_t count);

} // namespace oneui::capi

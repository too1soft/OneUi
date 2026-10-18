#include "oneui_c_api_controls_internal.h"
#include "oneui_c_api_error.h"
#include "oneui/controls/menu.h"
#include "oneui_c_api_window_internal.h"
#include "../internal/unicode.h"
#include <algorithm>
#include <cmath>
#include <climits>
#include <cstring>
#include <cwchar>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace oneui::capi;

namespace {

std::string menuString(const char* value,std::size_t limit) {
    if(!value)return {};
    std::size_t n=0;while(n<=limit && value[n])++n;
    if(n>limit)throw std::invalid_argument("Menu string exceeds limit");
    std::string text(value,n);
    if(oneui::unicode::toUtf8(oneui::unicode::fromUtf8(text))!=text)throw std::invalid_argument("Invalid menu UTF-8");
    return text;
}

std::vector<oneui::NativeMenuItem> copyMenu(const OneUiNativeMenuItemUtf8* items,std::size_t count,std::size_t& total,unsigned depth=1) {
    if(depth>8 || count>1024-total || (count&&!items))throw std::invalid_argument("Invalid menu tree");
    total+=count;std::vector<oneui::NativeMenuItem> result;result.reserve(count);
    for(std::size_t i=0;i<count;++i){const auto& in=items[i];
        if((in.flags&~31u) || (in.flags&12u)==12u)throw std::invalid_argument("Invalid menu flags");
        oneui::NativeMenuItem item;item.id=in.id;item.text=oneui::unicode::fromUtf8(menuString(in.text,16384));
        item.enabled=in.flags&1;item.separator=in.flags&2;item.check=(in.flags&8)?oneui::MenuCheck::Radio:(in.flags&4)?oneui::MenuCheck::Check:oneui::MenuCheck::Normal;
        item.checked=in.flags&16;item.radioGroup=in.radio_group;item.shortcut={menuString(in.shortcut_key,16),in.shortcut_modifiers};
        if(in.child_count)item.children=copyMenu(in.children,in.child_count,total,depth+1);
        result.push_back(std::move(item));
    }return result;
}

} // namespace

void oneui_window_set_close_to_tray(OneUiWindow* window, int close_to_tray) {
    if (!window || !window->window) {
        return;
    }
    window->window->setCloseToTray(close_to_tray != 0);
}

int oneui_window_set_tray_enabled(OneUiWindow* window, int enabled) {
    return window && window->window && window->window->setTrayEnabled(enabled != 0);
}

int oneui_window_set_tray_menu_utf8(OneUiWindow* window,const OneUiNativeMenuItemUtf8* items,size_t count) {
    try {if(!window||!window->window)return 0;std::size_t total=0;auto menu=copyMenu(items,count,total);return window->window->setTrayMenu(menu);}
    catch(...){oneui::capi::recordCurrentException(__func__);return 0;}
}

int oneui_window_set_menu_bar_utf8(OneUiWindow* window,const OneUiNativeMenuItemUtf8* items,size_t count) {
    try {if(!window||!window->window)return 0;std::size_t total=0;auto menu=copyMenu(items,count,total);return window->window->setMenuBar(menu);}
    catch(...){oneui::capi::recordCurrentException(__func__);return 0;}
}

void oneui_window_set_on_menu_action(OneUiWindow* window,OneUiIntCallback callback,void* data) {
    try {if(window&&window->window)window->window->setOnMenuAction(callback?std::function<void(int)>([callback,data](int id){callback(id,data);}):std::function<void(int)>{});}
    catch(...){oneui::capi::recordCurrentException(__func__);}
}

int oneui_window_set_tray_icon_rgba(OneUiWindow* window,const unsigned char* pixels,unsigned int width,unsigned int height,size_t stride,int templateImage) {
    try {
        if(!window||!window->window)return 0;
        oneui::TrayIcon icon;icon.width=width;icon.height=height;icon.templateImage=templateImage!=0;
        if(width==0 && height==0){if(pixels||stride)return 0;}
        else {
            if(!pixels || !width || !height || width>256 || height>256 || stride<width*4 || stride>16*1024*1024)return 0;
            icon.rgba.resize(std::size_t(width)*height*4);
            for(unsigned y=0;y<height;++y)std::memcpy(icon.rgba.data()+y*width*4,pixels+y*stride,width*4);
        }
        return window->window->setTrayIcon(icon);
    }catch(...){oneui::capi::recordCurrentException(__func__);return 0;}
}

int oneui_window_set_tray_menu(OneUiWindow* window, const OneUiTrayMenuItem* items, size_t count) {
    try {
    if (!window || !window->window || count > 1024 || (count && !items)) return 0;
    std::vector<oneui::TrayMenuItem> converted; converted.reserve(count);
    for (size_t i = 0; i < count; ++i)
        converted.push_back({items[i].id, items[i].text ? items[i].text : L"", items[i].enabled != 0, items[i].separator != 0});
    return window->window->setTrayMenu(converted);
    } catch (...) { oneui::capi::recordCurrentException(__func__); return 0; }
}

int oneui_window_set_tray_tooltip(OneUiWindow* window, const wchar_t* text) {
    return window && window->window && window->window->setTrayTooltip(text ? text : L"");
}

void oneui_window_set_on_tray_action(OneUiWindow* window, OneUiIntCallback callback, void* user_data) {
    if (!window || !window->window) return;
    window->window->setOnTrayAction(callback ? std::function<void(int)>([callback, user_data](int id) { callback(id, user_data); }) : std::function<void(int)>{});
}

int oneui_window_tray_icon_visible(OneUiWindow* window) {
    return window && window->window && window->window->trayIconVisible();
}

int oneui_window_notify_tray(OneUiWindow* window, const wchar_t* title, const wchar_t* message) {
    return window && window->window && window->window->notifyTray(title ? title : L"", message ? message : L"");
}

void oneui_window_set_on_close_requested(OneUiWindow* window, OneUiIntCallback callback, void* user_data) {
    if (!window || !window->window) return;
    if (!callback) { window->window->setOnCloseRequested({}); return; }
    window->window->setOnCloseRequested([callback, user_data](bool force) { callback(force ? 1 : 0, user_data); });
}

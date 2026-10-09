#pragma once
#include "oneui/command.h"
#include "oneui/export.h"
#include <cstdint>
#include <string>
#include <vector>

namespace oneui {
enum class MenuCheck { Normal = 0, Check = 1, Radio = 2 };
// Controlled state: activation reports an ID; applications update checked state.
// IDs are unique across actionable leaves. 0xE001/0xE002 are reserved for the
// default tray menu. Empty tray menus restore defaults; empty menu bars remove it.
struct NativeMenuItem {
    int id = 0;
    std::wstring text;
    bool enabled = true;
    bool separator = false;
    MenuCheck check = MenuCheck::Normal;
    bool checked = false;
    unsigned int radioGroup = 0;
    KeyChord shortcut;
    std::vector<NativeMenuItem> children;
};
using TrayMenuItem = NativeMenuItem;
// Copied straight-alpha RGBA8 pixels. Empty pixels/zero dimensions restore the
// default icon. Template rendering is a macOS hint; other platforms use colors.
struct TrayIcon {
    unsigned int width = 0, height = 0;
    std::vector<std::uint8_t> rgba;
    bool templateImage = false;
};
ONEUI_API bool validateNativeMenu(const std::vector<NativeMenuItem> &items);
ONEUI_API bool validateTrayIcon(const TrayIcon &icon);
ONEUI_API std::wstring menuShortcutLabel(const KeyChord &shortcut);
} // namespace oneui

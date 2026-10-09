#include "oneui/native_menu.h"
#include "internal/unicode.h"
#include "oneui/platform/key_mapping.h"
#include <algorithm>
#include <functional>
#include <map>
#include <set>

namespace oneui {
bool validateTrayIcon(const TrayIcon &icon) {
    return (icon.width == 0 && icon.height == 0 && icon.rgba.empty()) ||
           (icon.width > 0 && icon.height > 0 && icon.width <= 256 && icon.height <= 256 &&
            icon.rgba.size() == std::size_t(icon.width) * icon.height * 4);
}
bool validateNativeMenu(const std::vector<NativeMenuItem> &items) {
    std::size_t count = 0;
    std::set<int> ids;
    std::set<std::pair<std::string, unsigned int>> chords;
    const auto visit = [&](const auto &self, const std::vector<NativeMenuItem> &nodes,
                           int depth) -> bool {
        if (depth > 8 || nodes.size() > 1024 - count)
            return false;
        count += nodes.size();
        std::set<unsigned int> selectedGroups;
        for (const auto &item : nodes) {
            if (item.text.size() > 4096 || item.text.find(L'\0') != std::wstring::npos)
                return false;
            if (unicode::fromUtf8(unicode::toUtf8(item.text)) != item.text)
                return false;
            if (item.check != MenuCheck::Normal && item.check != MenuCheck::Check &&
                item.check != MenuCheck::Radio)
                return false;
            if ((item.checked && item.check == MenuCheck::Normal) ||
                (item.check == MenuCheck::Radio ? item.radioGroup == 0 : item.radioGroup != 0))
                return false;
            if (item.check == MenuCheck::Radio && item.checked &&
                !selectedGroups.insert(item.radioGroup).second)
                return false;
            if (item.separator) {
                if (!item.children.empty() || item.check != MenuCheck::Normal ||
                    !item.shortcut.key.empty() || item.shortcut.modifiers)
                    return false;
                continue;
            }
            if (item.text.empty())
                return false;
            if (!item.children.empty()) {
                if (item.check != MenuCheck::Normal || !item.shortcut.key.empty() ||
                    !self(self, item.children, depth + 1))
                    return false;
            } else if (item.id == 0xE001 || item.id == 0xE002 || !ids.insert(item.id).second)
                return false;
            if (item.shortcut.modifiers & ~31u)
                return false;
            auto key = item.shortcut.key;
            for (auto &c : key)
                if (c >= 'A' && c <= 'Z')
                    c += 'a' - 'A';
            // Portable, layout-independent shortcut vocabulary.
            bool valid = key.size() == 1 &&
                         ((key[0] >= 'a' && key[0] <= 'z') || (key[0] >= '0' && key[0] <= '9'));
            for (const auto *named :
                 {"enter", "escape", "space", "tab", "backspace", "delete", "left", "right", "up",
                  "down", "home", "end", "pageup", "pagedown"})
                valid |= key == named;
            for (int f = 1; f <= 24; ++f)
                valid |= key == "f" + std::to_string(f);
            if (!key.empty()) {
                unsigned int mods = item.shortcut.modifiers;
                if (mods & KeyModifierPrimary)
                    mods = (mods & ~KeyModifierPrimary) |
                           (platform::primaryUsesMeta ? KeyModifierMeta : KeyModifierControl);
                if (!valid || !chords.insert({key, mods}).second)
                    return false;
            } else if (item.shortcut.modifiers)
                return false;
        }
        return true;
    };
    return visit(visit, items, 1);
}
std::wstring menuShortcutLabel(const KeyChord &chord) {
    if (chord.key.empty())
        return {};
    unsigned int mods = chord.modifiers;
    if (mods & KeyModifierPrimary)
        mods = (mods & ~KeyModifierPrimary) |
               (platform::primaryUsesMeta ? KeyModifierMeta : KeyModifierControl);
    std::wstring result;
    if (mods & KeyModifierControl)
        result += L"Ctrl+";
    if (mods & KeyModifierAlt)
        result += L"Alt+";
    if (mods & KeyModifierShift)
        result += L"Shift+";
    if (mods & KeyModifierMeta)
        result += platform::primaryUsesMeta ? L"Cmd+" : L"Meta+";
    for (char c : chord.key)
        result += wchar_t(c >= 'a' && c <= 'z' ? c - ('a' - 'A') : c);
    return result;
}
} // namespace oneui

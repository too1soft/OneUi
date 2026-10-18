#include "oneui_c_api_controls_internal.h"
#include "oneui_c_api_error.h"
#include "oneui/fonts.h"
#include "oneui/platform/window.h"
#include "oneui/localization.h"
#include "../text/text_layout.h"
#include "../platform/shared/skia_canvas.h"
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

#include "oneui_c_api_fonts.inc"

int oneui_enumerate_system_fonts(OneUiFontFamilyCallback callback, void* user_data) {
    try {
        const auto fonts = oneui::enumerateSystemFonts();
        if (fonts.size() > static_cast<std::size_t>(INT_MAX)) return -1;
        if (callback) {
            for (const auto& font : fonts) {
                callback({font.family.data(), font.family.size()}, font.monospaced ? 1 : 0, user_data);
            }
        }
        return static_cast<int>(fonts.size());
    } catch (...) { oneui::capi::recordCurrentException(__func__); return -1; }
}

int oneui_font_register_memory(
    const uint8_t* data,
    size_t size,
    OneUiUtf8String family_alias) {
    try {
        if (!family_alias.data || family_alias.length == 0) {
            return 0;
        }
        const std::string rawAlias(family_alias.data, family_alias.length);
        if (rawAlias.find('\0') != std::string::npos) {
            return 0;
        }
        const auto alias = utf8FromWide(utf8OrEmpty(family_alias));
        if (alias != rawAlias) {
            return 0;
        }
        return oneui::rendering::registerFontFromMemory(data, size, alias) ? 1 : 0;
    } catch (...) {
        oneui::capi::recordCurrentException(__func__);
        return 0;
    }
}

int oneui_clipboard_set_text(const wchar_t* text) {
    try {
        oneui::SystemClipboard clipboard;
        const std::wstring value = wideOrEmpty(text);
        clipboard.setText(value);
        return clipboard.text() == value ? 1 : 0;
    } catch (...) {
        oneui::capi::recordCurrentException(__func__);
        return 0;
    }
}

int oneui_clipboard_get_text(wchar_t* buffer, int buffer_len) {
    try {
        oneui::SystemClipboard clipboard;
        const std::wstring value = clipboard.text();
        const int required = static_cast<int>(value.size()) + 1;
        if (buffer && buffer_len > 0) {
            const int count = std::min(buffer_len - 1, static_cast<int>(value.size()));
            std::copy_n(value.c_str(), count, buffer);
            buffer[count] = L'\0';
        }
        return required;
    } catch (...) {
        oneui::capi::recordCurrentException(__func__);
        if (buffer && buffer_len > 0) {
            buffer[0] = L'\0';
        }
        return 0;
    }
}

int oneui_clipboard_set_text_utf8(OneUiUtf8String text) {
    try {
        oneui::SystemClipboard clipboard;
        const std::wstring value = utf8OrEmpty(text);
        clipboard.setText(value);
        return clipboard.text() == value ? 1 : 0;
    } catch (...) {
        oneui::capi::recordCurrentException(__func__);
        return 0;
    }
}

std::size_t oneui_clipboard_get_text_utf8(char* buffer, std::size_t buffer_len) {
    try {
        oneui::SystemClipboard clipboard;
        const std::string value = utf8FromWide(clipboard.text());
        copyUtf8Field(value, buffer, buffer_len);
        return value.size() + 1;
    } catch (...) {
        oneui::capi::recordCurrentException(__func__);
        if (buffer && buffer_len > 0) {
            buffer[0] = '\0';
        }
        return 0;
    }
}

int oneui_set_builtin_text_utf8(int key, const char* text) {
    try {
        if (key < 0 || key > static_cast<int>(oneui::BuiltinText::FindClose)) throw std::invalid_argument("Unknown built-in text key");
        if (!text) return oneui::setBuiltinText(static_cast<oneui::BuiltinText>(key), std::nullopt);
        const auto length = std::char_traits<char>::length(text);
        if (length == 0 || length > 4096) throw std::invalid_argument("Built-in text must contain 1 to 4096 UTF-8 bytes");
        const auto wide = utf8OrEmpty(OneUiUtf8String{text, length});
        if (utf8FromWide(wide) != std::string(text, length)) throw std::invalid_argument("Invalid UTF-8 built-in text");
        if (!oneui::setBuiltinText(static_cast<oneui::BuiltinText>(key), wide))
            throw std::invalid_argument("Built-in text was rejected");
        return 1;
    } catch (...) {
        oneui::capi::recordCurrentException(__func__);
        return 0;
    }
}

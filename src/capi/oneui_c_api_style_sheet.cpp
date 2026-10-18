#include "oneui_c_api_controls_internal.h"
#include "oneui_c_api_error.h"
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

#include <fstream>
#include <filesystem>
namespace {

void copyError(const std::string& error, char* buffer, int len) {
    if (!buffer || len <= 0) {
        return;
    }
    const std::size_t size = static_cast<std::size_t>(len);
    const std::size_t count = std::min(size - 1, error.size());
    std::copy_n(error.c_str(), count, buffer);
    buffer[count] = '\0';
}

} // namespace

OneUiStyleSheet* oneui_style_sheet_create(void) {
    return new OneUiStyleSheet();
}

void oneui_style_sheet_destroy(OneUiStyleSheet* style_sheet) {
    delete style_sheet;
}

void oneui_style_sheet_set_transient_style_sheet(OneUiStyleSheet* style_sheet, OneUiStyleSheet* transient) {
    if (style_sheet && style_sheet->sheet) {
        style_sheet->sheet->setTransientStyleSheet(transient ? transient->sheet : nullptr);
    }
}

void oneui_style_sheet_set_custom_property(OneUiStyleSheet* style_sheet, const char* name, const char* value) {
    if (!style_sheet || !style_sheet->sheet || !name || !value) {
        return;
    }
    style_sheet->sheet->setCustomProperty(name, value);
}

int oneui_style_sheet_add_css(OneUiStyleSheet* style_sheet, const char* css, char* error_buffer, int error_buffer_len) {
    if (!style_sheet || !style_sheet->sheet || !css) {
        copyError("Invalid OneUI style sheet or CSS input", error_buffer, error_buffer_len);
        return 0;
    }
    std::string error;
    const bool ok = style_sheet->sheet->addRulesFromCss(css, &error);
    if (!ok) {
        copyError(error, error_buffer, error_buffer_len);
        return 0;
    }
    copyError("", error_buffer, error_buffer_len);
    return 1;
}

int oneui_style_sheet_replace_css(OneUiStyleSheet* style_sheet, const char* css, char* error_buffer, int error_buffer_len) {
    if (!style_sheet || !style_sheet->sheet || !css) {
        copyError("Invalid OneUI style sheet or CSS input", error_buffer, error_buffer_len);
        return 0;
    }
    oneui::StyleSheet replacement;
    std::string error;
    if (!replacement.addRulesFromCss(css, &error)) {
        copyError(error, error_buffer, error_buffer_len);
        return 0;
    }
    // Widget bindings and controls keep shared/weak references to this object.
    // Replace its contents, never its identity; invalid CSS leaves it intact.
    replacement.setTransientStyleSheet(style_sheet->sheet->transientStyleSheet());
    *style_sheet->sheet = std::move(replacement);
    copyError("", error_buffer, error_buffer_len);
    return 1;
}

int oneui_style_sheet_load_file(OneUiStyleSheet* style_sheet, const wchar_t* path, char* error_buffer, int error_buffer_len) {
    if (!style_sheet || !style_sheet->sheet || !path) {
        copyError("Invalid OneUI style sheet or file path", error_buffer, error_buffer_len);
        return 0;
    }
    std::ifstream file(std::filesystem::u8path(utf8FromWide(path)), std::ios::binary);
    if (!file) {
        copyError("Failed to open CSS file", error_buffer, error_buffer_len);
        return 0;
    }
    const std::string content{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    if (file.bad()) {
        copyError("Failed to read CSS file", error_buffer, error_buffer_len);
        return 0;
    }
    return oneui_style_sheet_add_css(style_sheet, content.c_str(), error_buffer, error_buffer_len);
}

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace oneui::text {
// Font settings accept either one family or an ordered CSS-style family list.
// Keep commas inside quoted family names and discard empty entries.
inline std::vector<std::string> fontFamilyList(std::string_view input) {
    std::vector<std::string> result;
    std::string entry;
    char quote = 0;
    bool escaped = false;
    auto append = [&] {
        const auto first = entry.find_first_not_of(" \t\r\n");
        if (first != std::string::npos) {
            auto family = entry.substr(first, entry.find_last_not_of(" \t\r\n") - first + 1);
#ifdef _WIN32
            if (family == "sans-serif" || family == "system-ui") family = "Segoe UI";
            else if (family == "serif") family = "Times New Roman";
            else if (family == "monospace") family = "Consolas";
#elif defined(__APPLE__)
            if (family == "sans-serif" || family == "system-ui") family = "Helvetica Neue";
            else if (family == "serif") family = "Times";
            else if (family == "monospace") family = "Menlo";
#endif
            result.push_back(std::move(family));
        }
        entry.clear();
    };
    for (char ch : input) {
        if (escaped) { entry += ch; escaped = false; }
        else if (ch == '\\') escaped = true;
        else if (quote) { if (ch == quote) quote = 0; else entry += ch; }
        else if (ch == '\'' || ch == '"') quote = ch;
        else if (ch == ',') append();
        else entry += ch;
    }
    if (escaped) entry += '\\';
    append();
    return result;
}
} // namespace oneui::text

#pragma once

#include <locale>
#include <string>
#include <string_view>
#include <vector>

namespace oneui::detail {

inline void appendStyleClasses(std::vector<std::string>& classes, std::string_view text) {
    if (text.empty()) return;
    const std::locale locale;
    const auto& whitespace = std::use_facet<std::ctype<char>>(locale);
    const auto* end = text.data() + text.size();
    auto* token = whitespace.scan_not(std::ctype_base::space, text.data(), end);
    while (token != end) {
        const auto* after = whitespace.scan_is(std::ctype_base::space, token, end);
        classes.emplace_back(token, after);
        token = whitespace.scan_not(std::ctype_base::space, after, end);
    }
}

} // namespace oneui::detail

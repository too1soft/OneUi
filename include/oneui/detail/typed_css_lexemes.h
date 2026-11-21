#pragma once

#include <string_view>

namespace oneui::ui::syntax::detail {
inline bool asciiLetter(char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

inline bool asciiDigit(char c) noexcept {
    return c >= '0' && c <= '9';
}

inline bool takeSelectorIdentifier(std::string_view& input, bool isClass) noexcept {
    if (input.empty() || !(asciiLetter(input.front()) || (isClass && input.front() == '_')))
        return false;
    std::size_t length = 1;
    while (length < input.size()) {
        const auto c = input[length];
        if (!(asciiLetter(c) || asciiDigit(c) || c == '_' || c == '-')) break;
        ++length;
    }
    input.remove_prefix(length);
    return true;
}

inline bool simpleSelector(std::string_view input, bool classOnly) noexcept {
    if (!classOnly && !takeSelectorIdentifier(input, false)) return false;
    while (!input.empty() && input.front() == '.') {
        input.remove_prefix(1);
        if (!takeSelectorIdentifier(input, true)) return false;
    }
    if (input.empty()) return true;
    if (classOnly) return false;
    return input == ":hover" || input == ":pressed" || input == ":focus" ||
           input == ":disabled" || input == ":selected" || input == ":checked" ||
           input == ":read-only";
}

inline bool takeDigits(std::string_view& input) noexcept {
    std::size_t length = 0;
    while (length < input.size() && asciiDigit(input[length])) ++length;
    input.remove_prefix(length);
    return length != 0;
}

inline bool lengthValue(std::string_view input, bool allowPercent = false) noexcept {
    if (!takeDigits(input)) return false;
    if (!input.empty() && input.front() == '.') {
        input.remove_prefix(1);
        if (!takeDigits(input)) return false;
    }
    return input == "px" || (allowPercent && input == "%");
}

inline bool integerValue(std::string_view input) noexcept {
    if (!input.empty() && input.front() == '-') input.remove_prefix(1);
    return takeDigits(input) && input.empty();
}
} // namespace oneui::ui::syntax::detail

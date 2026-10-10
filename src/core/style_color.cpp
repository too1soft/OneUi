#include "style_css_internal.h"
#include "internal/unicode.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace oneui {
using namespace style_detail;
std::optional<Color> parseStyleColor(const std::string& value) {
    std::string text = trim(value);
    const std::string lowered = lower(text);
    if (lowered == "transparent") {
        return Color{0, 0, 0, 0};
    }
    if (startsWith(lowered, "rgb(") || startsWith(lowered, "rgba(")) {
        const std::size_t open = text.find('(');
        const std::size_t close = text.rfind(')');
        if (open == std::string::npos || close == std::string::npos || close <= open) {
            return std::nullopt;
        }
        const auto parts = splitTopLevel(text.substr(open + 1, close - open - 1), ',');
        if (parts.size() != 3 && parts.size() != 4) {
            return std::nullopt;
        }

        auto parse_channel = [](const std::string& raw) -> std::optional<unsigned char> {
            std::string part = trim(raw);
            bool percent = false;
            if (!part.empty() && part.back() == '%') {
                percent = true;
                part.pop_back();
            }
            char* end = nullptr;
            const float parsed = std::strtof(part.c_str(), &end);
            if (!end || *end != '\0') {
                return std::nullopt;
            }
            const float value = percent ? parsed * 2.55f : parsed;
            return static_cast<unsigned char>(std::clamp(std::round(value), 0.0f, 255.0f));
        };

        auto parse_alpha = [](const std::string& raw) -> std::optional<unsigned char> {
            std::string part = trim(raw);
            bool percent = false;
            if (!part.empty() && part.back() == '%') {
                percent = true;
                part.pop_back();
            }
            char* end = nullptr;
            const float parsed = std::strtof(part.c_str(), &end);
            if (!end || *end != '\0') {
                return std::nullopt;
            }
            const float value = percent ? parsed * 2.55f : (parsed <= 1.0f ? parsed * 255.0f : parsed);
            return static_cast<unsigned char>(std::clamp(std::round(value), 0.0f, 255.0f));
        };

        auto r = parse_channel(parts[0]);
        auto g = parse_channel(parts[1]);
        auto b = parse_channel(parts[2]);
        auto a = parts.size() == 4 ? parse_alpha(parts[3]) : std::optional<unsigned char>{255};
        if (!r || !g || !b || !a) {
            return std::nullopt;
        }
        return Color{*r, *g, *b, *a};
    }

    if (text.empty() || text.front() != '#') {
        return std::nullopt;
    }
    text.erase(text.begin());
    if (text.size() != 6 && text.size() != 8) {
        return std::nullopt;
    }

    auto parse_byte = [](const std::string& part) -> std::optional<unsigned char> {
        char* end = nullptr;
        const long parsed = std::strtol(part.c_str(), &end, 16);
        if (!end || *end != '\0' || parsed < 0 || parsed > 255) {
            return std::nullopt;
        }
        return static_cast<unsigned char>(parsed);
    };

    auto r = parse_byte(text.substr(0, 2));
    auto g = parse_byte(text.substr(2, 2));
    auto b = parse_byte(text.substr(4, 2));
    auto a = text.size() == 8 ? parse_byte(text.substr(6, 2)) : std::optional<unsigned char>{255};
    if (!r || !g || !b || !a) {
        return std::nullopt;
    }
    return Color{*r, *g, *b, *a};
}

}

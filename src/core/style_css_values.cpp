#include "style_css_internal.h"
#include "internal/unicode.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace oneui::style_detail {
std::string trim(std::string value) {
    if (value.size() >= 3 &&
        static_cast<unsigned char>(value[0]) == 0xEF &&
        static_cast<unsigned char>(value[1]) == 0xBB &&
        static_cast<unsigned char>(value[2]) == 0xBF) {
        value.erase(0, 3);
    }
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();
    if (first >= last) {
        return {};
    }
    return std::string(first, last);
}

std::vector<std::string> split(const std::string& value, char delimiter) {
    std::vector<std::string> parts;
    std::stringstream stream(value);
    std::string part;
    while (std::getline(stream, part, delimiter)) {
        parts.push_back(part);
    }
    return parts;
}

std::vector<std::string> splitTopLevel(const std::string& value, char delimiter) {
    std::vector<std::string> parts;
    std::string part;
    int depth = 0;
    for (char ch : value) {
        if (ch == '(') {
            ++depth;
        } else if (ch == ')' && depth > 0) {
            --depth;
        }

        if (ch == delimiter && depth == 0) {
            parts.push_back(part);
            part.clear();
            continue;
        }
        part.push_back(ch);
    }
    parts.push_back(part);
    return parts;
}

std::vector<std::string> splitWhitespaceTopLevel(const std::string& value) {
    std::vector<std::string> parts;
    std::string part;
    int depth = 0;
    for (char ch : value) {
        if (ch == '(') {
            ++depth;
        } else if (ch == ')' && depth > 0) {
            --depth;
        }

        if (std::isspace(static_cast<unsigned char>(ch)) != 0 && depth == 0) {
            if (!part.empty()) {
                parts.push_back(part);
                part.clear();
            }
            continue;
        }
        part.push_back(ch);
    }
    if (!part.empty()) {
        parts.push_back(part);
    }
    return parts;
}

std::string removeComments(const std::string& css) {
    std::string out;
    for (std::size_t i = 0; i < css.size();) {
        if (i + 1 < css.size() && css[i] == '/' && css[i + 1] == '*') {
            i += 2;
            while (i + 1 < css.size() && !(css[i] == '*' && css[i + 1] == '/')) {
                ++i;
            }
            i = std::min(css.size(), i + 2);
        } else {
            out.push_back(css[i++]);
        }
    }
    return out;
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool startsWith(const std::string& value, const std::string& prefix) {
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

bool isCustomPropertyName(const std::string& name) {
    return name.size() > 2 && name[0] == '-' && name[1] == '-';
}

std::string normalizedCustomPropertyName(std::string name) {
    name = trim(std::move(name));
    return startsWith(name, "--") ? name : "--" + name;
}

std::string resolveCustomProperties(
    std::string value,
    const std::map<std::string, std::string>& customProperties,
    std::string* error) {
    std::size_t cursor = 0;
    while ((cursor = value.find("var(", cursor)) != std::string::npos) {
        const std::size_t close = value.find(')', cursor + 4);
        if (close == std::string::npos) {
            if (error) {
                *error = "Unclosed CSS variable reference: " + value;
            }
            return value;
        }

        const std::string body = value.substr(cursor + 4, close - cursor - 4);
        const auto parts = split(body, ',');
        const std::string name = normalizedCustomPropertyName(parts.empty() ? "" : parts.front());
        std::string replacement;
        auto found = customProperties.find(name);
        if (found != customProperties.end()) {
            replacement = found->second;
        } else if (parts.size() > 1) {
            replacement = trim(body.substr(body.find(',') + 1));
        } else {
            if (error) {
                *error = "Unknown CSS variable: " + name;
            }
            return value;
        }

        value.replace(cursor, close - cursor + 1, replacement);
        cursor += replacement.size();
    }
    return value;
}

std::optional<float> parsePx(const std::string& value) {
    std::string text = lower(trim(value));
    if (text.size() > 2 && text.substr(text.size() - 2) == "px") {
        text.resize(text.size() - 2);
    }
    char* end = nullptr;
    const float parsed = std::strtof(text.c_str(), &end);
    if (!end || *end != '\0') {
        return std::nullopt;
    }
    return parsed;
}

std::optional<float> parseDeg(const std::string& value) {
    std::string text = lower(trim(value));
    if (text.size() > 3 && text.substr(text.size() - 3) == "deg") {
        text.resize(text.size() - 3);
    }
    char* end = nullptr;
    const float parsed = std::strtof(text.c_str(), &end);
    if (!end || *end != '\0') {
        return std::nullopt;
    }
    return parsed;
}

std::optional<float> parseNumber(const std::string& value) {
    std::string text = lower(trim(value));
    char* end = nullptr;
    const float parsed = std::strtof(text.c_str(), &end);
    if (!end || *end != '\0') {
        return std::nullopt;
    }
    return parsed;
}

std::optional<double> parseDurationMs(const std::string& value) {
    std::string text = lower(trim(value));
    double multiplier = 1.0;
    if (text.size() > 2 && text.substr(text.size() - 2) == "ms") {
        text.resize(text.size() - 2);
    } else if (text.size() > 1 && text.back() == 's') {
        text.pop_back();
        multiplier = 1000.0;
    }

    char* end = nullptr;
    const double parsed = std::strtod(text.c_str(), &end);
    if (!end || *end != '\0' || parsed < 0.0) {
        return std::nullopt;
    }
    return parsed * multiplier;
}

std::optional<EasingCurve> parseEasingCurve(const std::string& value) {
    const std::string text = lower(trim(value));
    if (text == "linear") {
        return EasingCurve::Linear;
    }
    if (text == "ease-out" || text == "ease-out-cubic") {
        return EasingCurve::EaseOutCubic;
    }
    if (text == "ease-in-out" || text == "ease-in-out-cubic") {
        return EasingCurve::EaseInOutCubic;
    }
    return std::nullopt;
}

bool applyTransitionShorthand(StyleRule& rule, const std::string& value) {
    bool applied = false;
    for (const auto& token : splitWhitespaceTopLevel(value)) {
        const std::string text = lower(trim(token));
        if (text.empty() || text == "all" || text == "none") {
            continue;
        }
        if (auto duration = parseDurationMs(text)) {
            rule.box.transitionDurationMs = *duration;
            applied = true;
            continue;
        }
        if (auto easing = parseEasingCurve(text)) {
            rule.box.transitionEasing = *easing;
            applied = true;
            continue;
        }
    }
    return applied;
}

float clampOpacity(float value) {
    return std::max(0.0f, std::min(1.0f, value));
}

Color applyOpacity(Color color, std::optional<float> opacity) {
    if (!opacity) {
        return color;
    }
    color.a = static_cast<std::uint8_t>(
        std::max(0.0f, std::min(255.0f, static_cast<float>(color.a) * clampOpacity(*opacity))));
    return color;
}

std::optional<Insets> parseInsets(const std::string& value) {
    const auto parts = split(value, ' ');
    std::vector<float> nums;
    for (const auto& part : parts) {
        if (part.empty()) {
            continue;
        }
        if (auto parsed = parsePx(part)) {
            nums.push_back(*parsed);
        }
    }
    if (nums.size() == 1) {
        return Insets{nums[0], nums[0], nums[0], nums[0]};
    }
    if (nums.size() == 2) {
        return Insets{nums[0], nums[1], nums[0], nums[1]};
    }
    if (nums.size() == 4) {
        return Insets{nums[0], nums[1], nums[2], nums[3]};
    }
    return std::nullopt;
}

bool applyBorderShorthand(StyleRule& rule, const std::string& value, bool outline) {
    bool applied = false;
    for (const auto& token : splitWhitespaceTopLevel(value)) {
        if (auto width = parsePx(token)) {
            if (outline) {
                rule.box.outlineWidth = *width;
            } else {
                rule.box.borderWidth = *width;
            }
            applied = true;
            continue;
        }
        if (auto color = parseStyleColor(token)) {
            if (outline) {
                rule.box.outlineColor = *color;
            } else {
                rule.box.borderColor = *color;
            }
            applied = true;
        }
    }
    return applied;
}

std::optional<float> effectNumber(const std::string& token, const std::string& suffix) {
    if (token.size() <= suffix.size() || token.compare(token.size()-suffix.size(),suffix.size(),suffix)!=0) return {};
    const auto number=token.substr(0,token.size()-suffix.size()); char* end=nullptr;
    const float value=std::strtof(number.c_str(),&end);
    if (end==number.c_str() || *end || !std::isfinite(value)) return {};
    return value;
}

std::optional<Gradient> parseGradient(const std::string& value) {
    Gradient gradient;
    const auto open=value.find('(');
    if (open==value.npos || value.back()!=')') return {};
    const auto kind=lower(trim(value.substr(0,open)));
    if (kind!="linear-gradient" && kind!="radial-gradient") return {};
    gradient.radial=kind=="radial-gradient";
    auto parts=splitTopLevel(value.substr(open+1,value.size()-open-2),',');
    if (parts.size()<2) return {};
    size_t first=0;
    if (!parseStyleColor(trim(parts[0]))) {
        auto tokens=splitWhitespaceTopLevel(trim(parts[0]));
        // A first color with a position is a stop, not a descriptor.
        if (tokens.empty()) return {};
        if (!parseStyleColor(tokens[0])) {
            first=1;
            if (!gradient.radial) {
                if (tokens.size()!=1) return {};
                auto angle=effectNumber(tokens[0],"deg"); if(!angle)return {};gradient.angleDegrees=*angle;
            } else {
                size_t i=0;
                if(tokens[i]!="at") {auto radius=effectNumber(tokens[i++],"%");if(!radius || *radius<=0)return {};gradient.radius=*radius/100;}
                if(i<tokens.size()) {
                    if(tokens[i++]!="at" || tokens.size()-i!=2)return {};
                    auto x=effectNumber(tokens[i++],"%"),y=effectNumber(tokens[i],"%");if(!x || !y)return {};
                    gradient.center={*x/100,*y/100};
                }
            }
        }
    }
    if(parts.size()-first<2 || parts.size()-first>32)return {};
    for(size_t i=first;i<parts.size();++i) {
        auto tokens=splitWhitespaceTopLevel(trim(parts[i]));if(tokens.empty() || tokens.size()>2)return {};
        auto color=parseStyleColor(tokens[0]);if(!color)return {};
        float position=-1;
        if(tokens.size()==2) {auto p=effectNumber(tokens[1],"%");if(!p || *p<0 || *p>100)return {};position=*p/100;}
        gradient.stops.push_back({*color,position});
    }
    auto& stops=gradient.stops;
    if(stops.front().position<0)stops.front().position=0;
    if(stops.back().position<0)stops.back().position=1;
    size_t anchor=0;
    for(size_t i=1;i<stops.size();++i)if(stops[i].position>=0) {
        if(stops[i].position<stops[anchor].position)return {};
        for(size_t j=anchor+1;j<i;++j)stops[j].position=stops[anchor].position+(stops[i].position-stops[anchor].position)*float(j-anchor)/float(i-anchor);
        anchor=i;
    }
    return gradient;
}

}

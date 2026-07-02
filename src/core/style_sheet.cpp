#include "oneui/style_sheet.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace oneui {
namespace {

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

bool applyDeclaration(
    StyleRule& rule,
    const std::string& property,
    const std::string& raw_value,
    const std::map<std::string, std::string>& customProperties,
    std::string* error) {
    const std::string name = lower(trim(property));
    std::string variableError;
    const std::string value = trim(resolveCustomProperties(raw_value, customProperties, &variableError));
    if (!variableError.empty()) {
        if (error) {
            *error = variableError;
        }
        return false;
    }
    if (name.empty()) {
        return true;
    }
    if (isCustomPropertyName(name)) {
        return true;
    }

    if (name == "background" || name == "background-color") {
        const std::string lowered_value = lower(value);
        if (startsWith(lowered_value, "linear-gradient")) {
            std::vector<Color> colors;
            const std::size_t open = value.find('(');
            const std::size_t close = value.rfind(')');
            if (open != std::string::npos && close != std::string::npos && close > open) {
                const auto gradient_parts = split(value.substr(open + 1, close - open - 1), ',');
                if (!gradient_parts.empty()) {
                    const std::string first_part = trim(gradient_parts.front());
                    if (lower(first_part).find("deg") != std::string::npos) {
                        rule.box.background.gradientAngleDegrees = parseDeg(first_part);
                    }
                }
            }
            std::size_t pos = 0;
            while ((pos = value.find('#', pos)) != std::string::npos) {
                std::size_t end = pos + 1;
                while (end < value.size() && std::isxdigit(static_cast<unsigned char>(value[end]))) {
                    ++end;
                }
                if (auto color = parseStyleColor(value.substr(pos, end - pos))) {
                    colors.push_back(*color);
                }
                pos = end;
            }
            if (colors.size() >= 2) {
                rule.box.background.gradientStart = colors[0];
                rule.box.background.gradientEnd = colors[1];
                return true;
            }
        }
        if (auto color = parseStyleColor(value)) {
            rule.box.background.color = *color;
            return true;
        }
    } else if (name == "color") {
        if (auto color = parseStyleColor(value)) {
            rule.box.foreground = *color;
            return true;
        }
    } else if (name == "placeholder-color") {
        if (auto color = parseStyleColor(value)) {
            rule.box.placeholderColor = *color;
            return true;
        }
    } else if (name == "caret-color") {
        if (auto color = parseStyleColor(value)) {
            rule.box.caretColor = *color;
            return true;
        }
    } else if (name == "selection-color" || name == "selection-background" || name == "selection-background-color") {
        if (auto color = parseStyleColor(value)) {
            rule.box.selectionColor = *color;
            return true;
        }
    } else if (name == "border") {
        return applyBorderShorthand(rule, value, false);
    } else if (name == "border-color") {
        if (auto color = parseStyleColor(value)) {
            rule.box.borderColor = *color;
            return true;
        }
    } else if (name == "border-width") {
        if (auto width = parsePx(value)) {
            rule.box.borderWidth = *width;
            return true;
        }
    } else if (name == "outline") {
        return applyBorderShorthand(rule, value, true);
    } else if (name == "outline-color") {
        if (auto color = parseStyleColor(value)) {
            rule.box.outlineColor = *color;
            return true;
        }
    } else if (name == "outline-width") {
        if (auto width = parsePx(value)) {
            rule.box.outlineWidth = *width;
            return true;
        }
    } else if (name == "outline-offset") {
        if (auto offset = parsePx(value)) {
            rule.box.outlineOffset = *offset;
            return true;
        }
    } else if (name == "opacity") {
        if (auto opacity = parseNumber(value)) {
            rule.box.opacity = clampOpacity(*opacity);
            return true;
        }
    } else if (name == "content-background" || name == "content-background-color") {
        if (auto color = parseStyleColor(value)) {
            rule.box.content.backgroundColor = *color;
            return true;
        }
    } else if (name == "content-radius") {
        if (auto radius = parsePx(value)) {
            rule.box.content.radius = *radius;
            return true;
        }
    } else if (name == "content-inset") {
        if (auto inset = parseInsets(value)) {
            rule.box.content.inset = *inset;
            return true;
        }
    } else if (name == "border-radius") {
        if (auto radius = parsePx(value)) {
            rule.box.radius = *radius;
            return true;
        }
    } else if (name == "padding") {
        if (auto inset = parseInsets(value)) {
            rule.box.padding = *inset;
            return true;
        }
    } else if (name == "gap") {
        if (auto gap = parsePx(value)) {
            rule.box.gap = *gap;
            return true;
        }
    } else if (name == "width") {
        if (auto width = parsePx(value)) {
            rule.box.width = *width;
            return true;
        }
    } else if (name == "height") {
        if (auto height = parsePx(value)) {
            rule.box.height = *height;
            return true;
        }
    } else if (name == "font-size") {
        if (auto fontSize = parsePx(value)) {
            rule.box.fontSize = *fontSize;
            return true;
        }
    } else if (name == "font-weight") {
        if (auto fontWeight = parseNumber(value)) {
            rule.box.fontWeight = std::clamp(static_cast<int>(*fontWeight), 100, 900);
            return true;
        }
    } else if (name == "transition-duration") {
        if (auto duration = parseDurationMs(value)) {
            rule.box.transitionDurationMs = *duration;
            return true;
        }
    } else if (name == "transition-timing-function") {
        if (auto easing = parseEasingCurve(value)) {
            rule.box.transitionEasing = *easing;
            return true;
        }
    } else if (name == "transition") {
        return applyTransitionShorthand(rule, value);
    } else if (name == "box-shadow") {
        rule.box.shadows.clear();
        const auto shadows = splitTopLevel(value, ',');
        for (const auto& shadow_text : shadows) {
            auto tokens = splitWhitespaceTopLevel(trim(shadow_text));
            tokens.erase(std::remove_if(tokens.begin(), tokens.end(), [](const std::string& token) {
                return token.empty();
            }), tokens.end());
            if (tokens.size() < 4) {
                continue;
            }
            StyleShadow shadow;
            std::size_t cursor = 0;
            if (lower(tokens[cursor]) == "inset") {
                shadow.inset = true;
                ++cursor;
            }
            if (cursor + 3 >= tokens.size()) {
                continue;
            }
            auto x = parsePx(tokens[cursor++]);
            auto y = parsePx(tokens[cursor++]);
            auto blur = parsePx(tokens[cursor++]);
            float spread = 0.0f;
            std::optional<Color> color;
            if (cursor < tokens.size()) {
                if (auto maybe_spread = parsePx(tokens[cursor])) {
                    spread = *maybe_spread;
                    ++cursor;
                }
            }
            if (cursor < tokens.size()) {
                color = parseStyleColor(tokens[cursor]);
            }
            if (x && y && blur && color) {
                shadow.offset = Point{*x, *y};
                shadow.blurRadius = *blur;
                shadow.spreadRadius = spread;
                shadow.color = *color;
                rule.box.shadows.push_back(shadow);
            }
        }
        return true;
    }

    if (error) {
        *error = "Unsupported or invalid style declaration: " + property + ": " + raw_value;
    }
    return false;
}

struct ParsedSelector {
    std::string tag;
    std::vector<std::string> classes;
    StylePseudoMask pseudos = StyleStateNone;
    bool valid = false;
};

ParsedSelector parseSelector(const std::string& selector) {
    ParsedSelector parsed;
    const auto raw_parts = split(trim(selector), ':');
    if (raw_parts.empty()) {
        return parsed;
    }

    const auto base_parts = split(trim(raw_parts.front()), '.');
    if (!base_parts.empty() && !base_parts.front().empty()) {
        parsed.tag = trim(base_parts.front());
    }
    for (std::size_t index = 1; index < base_parts.size(); ++index) {
        const std::string klass = trim(base_parts[index]);
        if (!klass.empty()) {
            parsed.classes.push_back(klass);
        }
    }

    if (parsed.tag.empty() && parsed.classes.empty()) {
        return parsed;
    }

    for (std::size_t index = 1; index < raw_parts.size(); ++index) {
        parsed.pseudos |= parseStylePseudoState(trim(raw_parts[index]));
    }
    parsed.valid = true;
    return parsed;
}

bool hasClass(const StyleNode& node, const std::string& klass) {
    return std::find(node.classes.begin(), node.classes.end(), klass) != node.classes.end();
}

std::string resolveCacheKey(const StyleNode& node) {
    std::string key;
    key.reserve(node.tag.size() + node.classes.size() * 12 + 16);
    key.append(node.tag);
    key.push_back('\x1f');
    for (const auto& klass : node.classes) {
        key.append(klass);
        key.push_back('\x1e');
    }
    key.push_back('\x1f');
    key.append(std::to_string(node.state));
    return key;
}

} // namespace

void StyleSheet::addRule(StyleRule rule) {
    if (rule.order == 0) {
        rule.order = static_cast<int>(rules_.size()) + 1;
    }
    rules_.push_back(std::move(rule));
    resolveCache_.clear();
    ++version_;
}

void StyleSheet::setCustomProperty(std::string name, std::string value) {
    name = normalizedCustomPropertyName(std::move(name));
    if (name.size() <= 2) {
        return;
    }
    customProperties_[std::move(name)] = trim(std::move(value));
    resolveCache_.clear();
    ++version_;
}

std::optional<std::string> StyleSheet::customProperty(const std::string& name) const {
    const std::string key = normalizedCustomPropertyName(name);
    auto found = customProperties_.find(key);
    if (found == customProperties_.end()) {
        return std::nullopt;
    }
    return found->second;
}

bool StyleSheet::addRulesFromCss(const std::string& css, std::string* error) {
    const std::string text = removeComments(css);
    std::size_t cursor = 0;
    while (cursor < text.size()) {
        const std::size_t open = text.find('{', cursor);
        if (open == std::string::npos) {
            break;
        }
        const std::size_t close = text.find('}', open + 1);
        if (close == std::string::npos) {
            if (error) {
                *error = "Unclosed style rule block";
            }
            return false;
        }

        const std::string selector_text = trim(text.substr(cursor, open - cursor));
        const std::string body = text.substr(open + 1, close - open - 1);
        for (const auto& raw_selector : split(selector_text, ',')) {
            const std::string selector = trim(raw_selector);
            if (selector == ":root") {
                for (const auto& declaration : split(body, ';')) {
                    const std::size_t colon = declaration.find(':');
                    if (colon == std::string::npos) {
                        continue;
                    }
                    const std::string property = trim(declaration.substr(0, colon));
                    if (isCustomPropertyName(property)) {
                        setCustomProperty(property, declaration.substr(colon + 1));
                    }
                }
                continue;
            }

            StyleRule rule;
            rule.selector = selector;
            if (rule.selector.empty()) {
                continue;
            }
            for (const auto& declaration : split(body, ';')) {
                const std::size_t colon = declaration.find(':');
                if (colon == std::string::npos) {
                    continue;
                }
                if (!applyDeclaration(rule,
                                      declaration.substr(0, colon),
                                      declaration.substr(colon + 1),
                                      customProperties_,
                                      error)) {
                    return false;
                }
            }
            addRule(std::move(rule));
        }

        cursor = close + 1;
    }
    return true;
}

StyleBox StyleSheet::resolve(const StyleNode& node) const {
    const std::string cacheKey = resolveCacheKey(node);
    if (auto cached = resolveCache_.find(cacheKey); cached != resolveCache_.end()) {
        return cached->second;
    }

    struct Match {
        const StyleRule* rule = nullptr;
        int specificity = 0;
    };

    std::vector<Match> matches;
    for (const auto& rule : rules_) {
        if (selectorMatches(rule.selector, node)) {
            matches.push_back(Match{&rule, selectorSpecificity(rule.selector)});
        }
    }

    std::stable_sort(matches.begin(), matches.end(), [](const Match& lhs, const Match& rhs) {
        if (lhs.specificity != rhs.specificity) {
            return lhs.specificity < rhs.specificity;
        }
        return lhs.rule->order < rhs.rule->order;
    });

    StyleBox resolved;
    for (const auto& match : matches) {
        resolved = mergeStyleBox(std::move(resolved), match.rule->box);
    }
    resolveCache_.emplace(cacheKey, resolved);
    return resolved;
}

const std::vector<StyleRule>& StyleSheet::rules() const {
    return rules_;
}

const std::map<std::string, std::string>& StyleSheet::customProperties() const {
    return customProperties_;
}

std::size_t StyleSheet::version() const {
    return version_;
}

StylePseudoMask parseStylePseudoState(const std::string& pseudo) {
    if (pseudo == "hover") {
        return StyleStateHover;
    }
    if (pseudo == "active") {
        return StyleStateActive;
    }
    if (pseudo == "focus") {
        return StyleStateFocus;
    }
    if (pseudo == "disabled") {
        return StyleStateDisabled;
    }
    if (pseudo == "selected" || pseudo == "checked") {
        return StyleStateSelected;
    }
    if (pseudo == "read-only" || pseudo == "readonly") {
        return StyleStateReadOnly;
    }
    return StyleStateNone;
}

std::optional<Color> parseStyleColor(const std::string& value) {
    std::string text = trim(value);
    const std::string lowered = lower(text);
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

bool selectorMatches(const std::string& selector, const StyleNode& node) {
    const ParsedSelector parsed = parseSelector(selector);
    if (!parsed.valid) {
        return false;
    }
    if (!parsed.tag.empty() && parsed.tag != node.tag) {
        return false;
    }
    for (const auto& klass : parsed.classes) {
        if (!hasClass(node, klass)) {
            return false;
        }
    }
    return (node.state & parsed.pseudos) == parsed.pseudos;
}

int selectorSpecificity(const std::string& selector) {
    const ParsedSelector parsed = parseSelector(selector);
    if (!parsed.valid) {
        return 0;
    }

    int pseudo_count = 0;
    StylePseudoMask pseudos = parsed.pseudos;
    while (pseudos != 0) {
        pseudo_count += static_cast<int>(pseudos & 1u);
        pseudos >>= 1;
    }

    return (parsed.tag.empty() ? 0 : 1) +
           static_cast<int>(parsed.classes.size()) * 10 +
           pseudo_count * 10;
}

StyleBox mergeStyleBox(StyleBox base, const StyleBox& overlay) {
    if (overlay.background.color) {
        base.background.color = overlay.background.color;
    }
    if (overlay.background.gradientStart) {
        base.background.gradientStart = overlay.background.gradientStart;
    }
    if (overlay.background.gradientEnd) {
        base.background.gradientEnd = overlay.background.gradientEnd;
    }
    if (overlay.background.gradientAngleDegrees) {
        base.background.gradientAngleDegrees = overlay.background.gradientAngleDegrees;
    }
    if (overlay.content.backgroundColor) {
        base.content.backgroundColor = overlay.content.backgroundColor;
    }
    if (overlay.content.radius) {
        base.content.radius = overlay.content.radius;
    }
    if (overlay.content.inset) {
        base.content.inset = overlay.content.inset;
    }
    if (overlay.foreground) {
        base.foreground = overlay.foreground;
    }
    if (overlay.placeholderColor) {
        base.placeholderColor = overlay.placeholderColor;
    }
    if (overlay.caretColor) {
        base.caretColor = overlay.caretColor;
    }
    if (overlay.selectionColor) {
        base.selectionColor = overlay.selectionColor;
    }
    if (overlay.borderColor) {
        base.borderColor = overlay.borderColor;
    }
    if (overlay.borderWidth) {
        base.borderWidth = overlay.borderWidth;
    }
    if (overlay.outlineColor) {
        base.outlineColor = overlay.outlineColor;
    }
    if (overlay.outlineWidth) {
        base.outlineWidth = overlay.outlineWidth;
    }
    if (overlay.outlineOffset) {
        base.outlineOffset = overlay.outlineOffset;
    }
    if (overlay.opacity) {
        base.opacity = overlay.opacity;
    }
    if (overlay.radius) {
        base.radius = overlay.radius;
    }
    if (overlay.padding) {
        base.padding = overlay.padding;
    }
    if (overlay.gap) {
        base.gap = overlay.gap;
    }
    if (overlay.width) {
        base.width = overlay.width;
    }
    if (overlay.height) {
        base.height = overlay.height;
    }
    if (overlay.fontSize) {
        base.fontSize = overlay.fontSize;
    }
    if (overlay.fontWeight) {
        base.fontWeight = overlay.fontWeight;
    }
    if (overlay.transitionDurationMs) {
        base.transitionDurationMs = overlay.transitionDurationMs;
    }
    if (overlay.transitionEasing) {
        base.transitionEasing = overlay.transitionEasing;
    }
    if (!overlay.shadows.empty()) {
        base.shadows = overlay.shadows;
    }
    return base;
}

Rect styleContentRect(Rect rect, const StyleBox& box) {
    return box.content.inset ? rect.inset(*box.content.inset) : rect;
}

void paintStyleBox(Canvas& canvas, Rect rect, const StyleBox& box) {
    const float radius = box.radius.value_or(0.0f);
    for (const auto& shadow : box.shadows) {
        if (!shadow.inset) {
            StyleShadow paintedShadow = shadow;
            paintedShadow.color = applyOpacity(paintedShadow.color, box.opacity);
            canvas.drawBoxShadow(
                Rect{
                    rect.x + paintedShadow.offset.x,
                    rect.y + paintedShadow.offset.y,
                    rect.width,
                    rect.height},
                BoxShadow{paintedShadow.color, Point{0.0f, 0.0f}, paintedShadow.blurRadius, paintedShadow.spreadRadius},
                radius);
        }
    }

    if (box.background.color) {
        canvas.fillRect(rect, applyOpacity(*box.background.color, box.opacity), radius);
    } else if (box.background.gradientStart && box.background.gradientEnd) {
        canvas.fillLinearGradient(
            rect,
            applyOpacity(*box.background.gradientStart, box.opacity),
            applyOpacity(*box.background.gradientEnd, box.opacity),
            box.background.gradientAngleDegrees.value_or(180.0f),
            radius);
    } else if (box.background.gradientStart) {
        canvas.fillRect(rect, applyOpacity(*box.background.gradientStart, box.opacity), radius);
    }
    if (box.content.backgroundColor) {
        canvas.fillRect(styleContentRect(rect, box), applyOpacity(*box.content.backgroundColor, box.opacity), box.content.radius.value_or(radius));
    }
    if (box.borderColor) {
        canvas.strokeRect(rect, applyOpacity(*box.borderColor, box.opacity), radius, box.borderWidth.value_or(1.0f));
    }
    for (const auto& shadow : box.shadows) {
        if (shadow.inset) {
            canvas.strokeRect(rect, applyOpacity(shadow.color, box.opacity), radius, std::max(1.0f, shadow.blurRadius));
        }
    }
    if (box.outlineColor && box.outlineWidth) {
        const float width = std::max(1.0f, *box.outlineWidth);
        const float offset = box.outlineOffset.value_or(0.0f) + width * 0.5f;
        canvas.strokeRect(
            Rect{rect.x - offset, rect.y - offset, rect.width + offset * 2.0f, rect.height + offset * 2.0f},
            applyOpacity(*box.outlineColor, box.opacity),
            radius + offset,
            width);
    }
}

} // namespace oneui

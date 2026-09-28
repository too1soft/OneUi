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
        if (startsWith(lowered_value,"radial-gradient") || startsWith(lowered_value,"linear-gradient")) {
            const auto gradient=parseGradient(value);
            if(!gradient) {if(error)*error="Invalid gradient: expected 2-32 ordered color stops (0-100%) and a supported descriptor";return false;}
            rule.box.background={};rule.box.background.gradient=*gradient;
            rule.box.background.gradientStart=gradient->stops.front().color;
            rule.box.background.gradientEnd=gradient->stops.back().color;
            rule.box.background.gradientAngleDegrees=gradient->angleDegrees;
            if(gradient->radial){rule.box.background.radialCenter=gradient->center;rule.box.background.radialRadius=gradient->radius;}
            return true;
        }
        if (auto color = parseStyleColor(value)) {
            rule.box.background={};
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
    } else if (name == "grid-min-column-width") {
        if (auto width = parsePx(value)) {
            rule.box.gridMinColumnWidth = *width;
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
    } else if (name == "detail-font-size") {
        if (auto fontSize = parsePx(value)) {
            rule.box.detailFontSize = *fontSize;
            return true;
        }
    } else if (name == "detail-font-weight") {
        if (auto fontWeight = parseNumber(value)) {
            rule.box.detailFontWeight = std::clamp(static_cast<int>(*fontWeight), 100, 900);
            return true;
        }
    } else if (name == "icon-size") {
        if (auto size = parsePx(value); size && *size >= 0.0f) {
            rule.box.iconSize = *size;
            return true;
        }
    } else if (name == "text-inset") {
        if (auto inset = parsePx(value)) {
            rule.box.textInset = *inset;
            return true;
        }
    } else if (name == "title-offset-y") {
        if (auto offset = parsePx(value)) {
            rule.box.titleOffsetY = *offset;
            return true;
        }
    } else if (name == "detail-offset-y") {
        if (auto offset = parsePx(value)) {
            rule.box.detailOffsetY = *offset;
            return true;
        }
    } else if (name == "scrollbar-color") {
        if (auto color = parseStyleColor(value)) {
            rule.box.scrollbarColor = *color;
            return true;
        }
    } else if (name == "scrollbar-width") {
        if (auto width = parsePx(value)) {
            rule.box.scrollbarWidth = *width;
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
        std::vector<StyleShadow> parsed;
        if(lower(value)!="none")for(const auto& part:splitTopLevel(value,',')) {
            auto tokens=splitWhitespaceTopLevel(trim(part));StyleShadow shadow;size_t i=0;
            if(!tokens.empty() && lower(tokens[0])=="inset"){shadow.inset=true;++i;}
            const auto count=tokens.size()-i;
            auto fail=[&]{if(error)*error="Invalid box-shadow: expected [inset] xpx ypx blurpx [spreadpx] color, or none";return false;};
            if(count!=4 && count!=5)return fail();
            auto x=effectNumber(tokens[i++],"px"),y=effectNumber(tokens[i++],"px"),blur=effectNumber(tokens[i++],"px");
            std::optional<float> spread=0.0f;if(count==5)spread=effectNumber(tokens[i++],"px");
            auto color=parseStyleColor(tokens[i]);
            if(!x || !y || !blur || *blur<0 || !spread || !color)return fail();
            shadow.offset={*x,*y};shadow.blurRadius=*blur;shadow.spreadRadius=*spread;shadow.color=*color;parsed.push_back(shadow);
            if(parsed.size()>8)return fail();
        }
        rule.box.shadows=std::move(parsed);rule.box.shadowsSpecified=true;
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
        const auto pseudo = parseStylePseudoState(trim(raw_parts[index]));
        if (pseudo == StyleStateNone) {
            return parsed;
        }
        parsed.pseudos |= pseudo;
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
                std::string property = trim(declaration.substr(0, colon));
                std::string value = trim(declaration.substr(colon + 1));
                if (!applyDeclaration(rule, property, value,
                                      customProperties_,
                                      error)) {
                    return false;
                }
                rule.declarations.emplace_back(std::move(property), std::move(value));
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
        StyleBox current = match.rule->box;
        if (!match.rule->declarations.empty()) {
            StyleRule dynamicRule;
            for (const auto& [property, value] : match.rule->declarations) {
                // The declaration was validated when the CSS was loaded. A
                // failed runtime resolution leaves the last valid parsed box
                // in place instead of partially applying a theme.
                if (!applyDeclaration(
                        dynamicRule, property, value, customProperties_, nullptr)) {
                    dynamicRule.box = match.rule->box;
                    break;
                }
            }
            current = mergeStyleBox(std::move(current), dynamicRule.box);
        }
        resolved = mergeStyleBox(std::move(resolved), current);
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
    if (pseudo == "focus" || pseudo == "focus-visible") {
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
    if (overlay.background.color || overlay.background.gradient || overlay.background.gradientStart) {
        base.background=overlay.background;
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
    if (overlay.gridMinColumnWidth) {
        base.gridMinColumnWidth = overlay.gridMinColumnWidth;
    }
    if (overlay.fontSize) {
        base.fontSize = overlay.fontSize;
    }
    if (overlay.fontWeight) {
        base.fontWeight = overlay.fontWeight;
    }
    if (overlay.detailFontSize) {
        base.detailFontSize = overlay.detailFontSize;
    }
    if (overlay.detailFontWeight) {
        base.detailFontWeight = overlay.detailFontWeight;
    }
    if (overlay.iconSize) base.iconSize = overlay.iconSize;
    if (overlay.textInset) {
        base.textInset = overlay.textInset;
    }
    if (overlay.titleOffsetY) {
        base.titleOffsetY = overlay.titleOffsetY;
    }
    if (overlay.detailOffsetY) {
        base.detailOffsetY = overlay.detailOffsetY;
    }
    if (overlay.scrollbarColor) {
        base.scrollbarColor = overlay.scrollbarColor;
    }
    if (overlay.scrollbarWidth) {
        base.scrollbarWidth = overlay.scrollbarWidth;
    }
    if (overlay.transitionDurationMs) {
        base.transitionDurationMs = overlay.transitionDurationMs;
    }
    if (overlay.transitionEasing) {
        base.transitionEasing = overlay.transitionEasing;
    }
    if (overlay.shadowsSpecified || !overlay.shadows.empty()) {
        base.shadowsSpecified = true;
        base.shadows = overlay.shadows;
    }
    return base;
}

Rect styleContentRect(Rect rect, const StyleBox& box) {
    return box.content.inset ? rect.inset(*box.content.inset) : rect;
}

Rect stylePaintBounds(Rect rect,const StyleBox& box) {
    float left=0,right=0,top=0,bottom=0;
    for(const auto& s:box.shadows)if(!s.inset && s.color.a) {
        const float pad=std::max(0.0f,s.blurRadius*3+s.spreadRadius+4);
        left=std::max(left,pad-s.offset.x);right=std::max(right,pad+s.offset.x);
        top=std::max(top,pad-s.offset.y);bottom=std::max(bottom,pad+s.offset.y);
    }
    return {rect.x-left,rect.y-top,rect.width+left+right,rect.height+top+bottom};
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
    } else if (box.background.gradient) {
        auto gradient=*box.background.gradient;
        for(auto& stop:gradient.stops)stop.color=applyOpacity(stop.color,box.opacity);
        canvas.fillGradient(rect,gradient,radius);
    } else if (box.background.gradientStart && box.background.gradientEnd) {
        if (box.background.radialCenter) {
            canvas.fillRadialGradient(
                rect,
                applyOpacity(*box.background.gradientStart, box.opacity),
                applyOpacity(*box.background.gradientEnd, box.opacity),
                *box.background.radialCenter,
                box.background.radialRadius.value_or(0.75f),
                radius);
        } else {
            canvas.fillLinearGradient(
                rect,
                applyOpacity(*box.background.gradientStart, box.opacity),
                applyOpacity(*box.background.gradientEnd, box.opacity),
                box.background.gradientAngleDegrees.value_or(180.0f),
                radius);
        }
    } else if (box.background.gradientStart) {
        canvas.fillRect(rect, applyOpacity(*box.background.gradientStart, box.opacity), radius);
    }
    if (box.content.backgroundColor) {
        canvas.fillRect(styleContentRect(rect, box), applyOpacity(*box.content.backgroundColor, box.opacity), box.content.radius.value_or(radius));
    }
    // border-width: 0 语义为“无边框”：宽度 0 传给 Skia 会画 1px 发丝线（hairline），必须显式跳过。
    if (box.borderColor && box.borderWidth.value_or(1.0f) > 0.0f) {
        canvas.strokeRect(rect, applyOpacity(*box.borderColor, box.opacity), radius, box.borderWidth.value_or(1.0f));
    }
    for (const auto& shadow : box.shadows) {
        if (shadow.inset) {
            canvas.drawInsetShadow(rect,BoxShadow{applyOpacity(shadow.color,box.opacity),shadow.offset,shadow.blurRadius,shadow.spreadRadius},radius);
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

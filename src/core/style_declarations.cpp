#include "style_css_internal.h"
#include "internal/unicode.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace oneui::style_detail {
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
    if(name=="container-name") {
        if(!value.empty() && std::all_of(value.begin(),value.end(),[](unsigned char c){return std::isalnum(c)||c=='_'||c=='-';})) {rule.box.containerName=value;return true;}
    } else if(name=="container-type") {
        if(value=="size"){rule.box.sizeContainment=true;return true;}
    } else if(name=="display") {
        if(value=="none"||value=="flex"){rule.box.display=value=="flex";return true;}
    } else if(name=="flex-direction") {
        if(value=="row"||value=="column"){rule.box.flexDirection=value;return true;}
    } else if(name=="flex-wrap") {
        if(value=="wrap"||value=="nowrap"){rule.box.flexWrap=value=="wrap";return true;}
    } else if(name=="flex-basis"||name=="min-width"||name=="min-height") {
        if(auto number=parsePx(value);number&&*number>=0) {
            (name=="flex-basis"?rule.box.flexBasis:name=="min-width"?rule.box.minWidth:rule.box.minHeight)=*number;return true;
        }
    } else if(name=="flex-grow"||name=="flex-shrink") {
        if(auto number=parseNumber(value);number&&*number>=0) {
            (name=="flex-grow"?rule.box.flexGrow:rule.box.flexShrink)=*number;return true;
        }
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
    } else if (name == "background-opacity" || name == "content-background-opacity") {
        std::string number = trim(value);
        const bool percentage = !number.empty() && number.back() == '%';
        if (percentage) number.pop_back();
        if (trim(number).empty()) return false;
        if (auto opacity = parseNumber(number); opacity && std::isfinite(*opacity)) {
            const float amount = clampOpacity(*opacity / (percentage ? 100.0f : 1.0f));
            if (name == "background-opacity") rule.box.backgroundOpacity = amount;
            else rule.box.contentBackgroundOpacity = amount;
            return true;
        }
        if (error) *error = "Invalid background opacity: expected a finite number or percentage";
        return false;
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
    } else if (name == "font-family") {
        if (!value.empty()) {
            rule.box.fontFamily = value == "inherit" ? std::wstring{} : unicode::fromUtf8(value);
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
    } else if (name == "font-variant-numeric") {
        const auto numeric = lower(trim(value));
        if (numeric == "tabular-nums" || numeric == "normal" || numeric == "proportional-nums") {
            rule.box.tabularNumbers = numeric == "tabular-nums"; return true;
        }
    } else if (name == "letter-spacing") {
        if (lower(trim(value)) == "normal") { rule.box.letterSpacing = 0.0f; return true; }
        if (auto spacing = parsePx(value); spacing && std::isfinite(*spacing)) {
            rule.box.letterSpacing = *spacing; return true;
        }
    } else if (name == "tab-indicator-color") {
        if (auto color = parseStyleColor(value)) { rule.box.tabIndicatorColor = *color; return true; }
    } else if (name == "tab-indicator-inset" || name == "tab-indicator-height" ||
               name == "tab-outline-width" || name == "tab-trailing-inset") {
        if (auto size = parsePx(value); size && std::isfinite(*size) && *size >= 0.0f) {
            if (name == "tab-indicator-inset") rule.box.tabIndicatorInset = *size;
            else if (name == "tab-indicator-height") rule.box.tabIndicatorHeight = *size;
            else if (name == "tab-outline-width") rule.box.tabOutlineWidth = *size;
            else rule.box.tabTrailingInset = *size;
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
    } else if (name == "check-background") {
        if (auto color = parseStyleColor(value)) { rule.box.checkBackground = *color; return true; }
    } else if (name == "check-border") {
        if (auto color = parseStyleColor(value)) { rule.box.checkBorder = *color; return true; }
    } else if (name == "check-radius") {
        if (auto radius = parsePx(value)) { rule.box.checkRadius = std::clamp(*radius, 0.0f, 8.0f); return true; }
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
}

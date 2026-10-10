#pragma once
#include "oneui/style_sheet.h"
namespace oneui::style_detail {
std::string trim(std::string value);
std::vector<std::string> split(const std::string& value, char delimiter);
std::vector<std::string> splitTopLevel(const std::string& value, char delimiter);
std::vector<std::string> splitWhitespaceTopLevel(const std::string& value);
std::string removeComments(const std::string& css);
std::string lower(std::string value);
bool startsWith(const std::string& value, const std::string& prefix);
bool isCustomPropertyName(const std::string& name);
std::string normalizedCustomPropertyName(std::string name);
std::string resolveCustomProperties(
    std::string value,
    const std::map<std::string, std::string>& customProperties,
    std::string* error);
std::optional<float> parsePx(const std::string& value);
std::optional<float> parseDeg(const std::string& value);
std::optional<float> parseNumber(const std::string& value);
std::optional<double> parseDurationMs(const std::string& value);
std::optional<EasingCurve> parseEasingCurve(const std::string& value);
bool applyTransitionShorthand(StyleRule& rule, const std::string& value);
float clampOpacity(float value);
Color applyOpacity(Color color, std::optional<float> opacity);
std::optional<Insets> parseInsets(const std::string& value);
bool applyBorderShorthand(StyleRule& rule, const std::string& value, bool outline);
std::optional<float> effectNumber(const std::string& token, const std::string& suffix);
std::optional<Gradient> parseGradient(const std::string& value);
struct ParsedSelector {
    std::string tag;
    std::vector<std::string> classes;
    StylePseudoMask pseudos = StyleStateNone;
    bool valid = false;
};

ParsedSelector parseSelector(const std::string&);
bool hasClass(const StyleNode&,const std::string&);
std::string resolveCacheKey(const StyleNode&);
bool applyDeclaration(StyleRule&,const std::string&,const std::string&,const std::map<std::string,std::string>&,std::string*);
}

#pragma once

#include "oneui/animation.h"
#include "oneui/canvas.h"
#include "oneui/color.h"
#include "oneui/export.h"
#include "oneui/geometry.h"

#include <cstddef>
#include <optional>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace oneui {

enum StylePseudoState : unsigned int {
    StyleStateNone = 0,
    StyleStateHover = 1u << 0,
    StyleStateActive = 1u << 1,
    StyleStateFocus = 1u << 2,
    StyleStateDisabled = 1u << 3,
    StyleStateSelected = 1u << 4,
    StyleStateReadOnly = 1u << 5
};

using StylePseudoMask = unsigned int;

struct StyleShadow {
    Color color{0, 0, 0, 0};
    Point offset{};
    float blurRadius = 0.0f;
    float spreadRadius = 0.0f;
    bool inset = false;
};

struct StyleBackground {
    std::optional<Color> color;
    std::optional<Color> gradientStart;
    std::optional<Color> gradientEnd;
    std::optional<float> gradientAngleDegrees;
};

struct StyleContentBox {
    std::optional<Color> backgroundColor;
    std::optional<float> radius;
    std::optional<Insets> inset;
};

struct StyleBox {
    StyleBackground background;
    StyleContentBox content;
    std::optional<Color> foreground;
    std::optional<Color> placeholderColor;
    std::optional<Color> caretColor;
    std::optional<Color> selectionColor;
    std::optional<Color> borderColor;
    std::optional<float> borderWidth;
    std::optional<Color> outlineColor;
    std::optional<float> outlineWidth;
    std::optional<float> outlineOffset;
    std::optional<float> opacity;
    std::optional<float> radius;
    std::optional<Insets> padding;
    std::optional<float> gap;
    std::optional<float> width;
    std::optional<float> height;
    std::optional<float> fontSize;
    std::optional<int> fontWeight;
    std::optional<double> transitionDurationMs;
    std::optional<EasingCurve> transitionEasing;
    std::vector<StyleShadow> shadows;
};

struct StyleRule {
    std::string selector;
    StyleBox box;
    int order = 0;
};

struct StyleNode {
    std::string tag;
    std::vector<std::string> classes;
    StylePseudoMask state = StyleStateNone;
};

class ONEUI_API StyleSheet {
public:
    void addRule(StyleRule rule);
    void setCustomProperty(std::string name, std::string value);
    std::optional<std::string> customProperty(const std::string& name) const;
    bool addRulesFromCss(const std::string& css, std::string* error = nullptr);
    StyleBox resolve(const StyleNode& node) const;
    const std::vector<StyleRule>& rules() const;
    const std::map<std::string, std::string>& customProperties() const;
    std::size_t version() const;

private:
    std::vector<StyleRule> rules_;
    std::map<std::string, std::string> customProperties_;
    mutable std::unordered_map<std::string, StyleBox> resolveCache_;
    std::size_t version_ = 0;
};

ONEUI_API StylePseudoMask parseStylePseudoState(const std::string& pseudo);
ONEUI_API std::optional<Color> parseStyleColor(const std::string& value);
ONEUI_API bool selectorMatches(const std::string& selector, const StyleNode& node);
ONEUI_API int selectorSpecificity(const std::string& selector);
ONEUI_API StyleBox mergeStyleBox(StyleBox base, const StyleBox& overlay);
ONEUI_API Rect styleContentRect(Rect rect, const StyleBox& box);
ONEUI_API void paintStyleBox(Canvas& canvas, Rect rect, const StyleBox& box);

} // namespace oneui

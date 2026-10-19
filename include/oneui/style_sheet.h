#pragma once

#include "oneui/animation.h"
#include "oneui/canvas.h"
#include "oneui/color.h"
#include "oneui/export.h"
#include "oneui/geometry.h"

#include <cstddef>
#include <optional>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace oneui {
struct ContainerQuery;

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
    // radial-gradient：圆心（相对 rect 的 0-1 归一化坐标）与半径（相对 max(宽,高) 的比例）。
    // radialCenter 有值时 gradientStart/End 按径向渐变绘制，否则走线性。
    std::optional<Point> radialCenter;
    std::optional<float> radialRadius;
    std::optional<Gradient> gradient;
};

struct StyleContentBox {
    std::optional<Color> backgroundColor;
    std::optional<float> radius;
    std::optional<Insets> inset;
};

struct StyleBox {
    std::optional<std::string> containerName,flexDirection;
    std::optional<bool> sizeContainment,display,flexWrap;
    std::optional<float> flexBasis,flexGrow,flexShrink,minWidth,minHeight;
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
    std::optional<float> gridMinColumnWidth;
    std::optional<float> fontSize;
    std::optional<std::wstring> fontFamily;
    std::optional<int> fontWeight;
    std::optional<float> detailFontSize;
    std::optional<int> detailFontWeight;
    std::optional<float> textInset;
    std::optional<float> letterSpacing;
    std::optional<bool> tabularNumbers;
    std::optional<Color> tabIndicatorColor;
    std::optional<float> tabIndicatorInset;
    std::optional<float> tabIndicatorHeight;
    std::optional<float> tabOutlineWidth;
    std::optional<float> tabTrailingInset;
    std::optional<float> iconSize;
    std::optional<float> titleOffsetY;
    std::optional<float> detailOffsetY;
    std::optional<Color> checkBackground;
    std::optional<Color> checkBorder;
    std::optional<float> checkRadius;
    std::optional<Color> scrollbarColor;
    std::optional<float> scrollbarWidth;
    std::optional<double> transitionDurationMs;
    std::optional<EasingCurve> transitionEasing;
    std::vector<StyleShadow> shadows;
    bool shadowsSpecified = false;
    // Background-only multipliers, consumed after the complete cascade. They
    // never dim text, icons, borders, focus rings or child widgets.
    std::optional<float> backgroundOpacity;
    std::optional<float> contentBackgroundOpacity;
};

// New presentation properties are separate from the frozen StyleBox ABI.
struct StyleLayout {
    std::optional<float> maxWidth,maxHeight;
    std::optional<std::vector<float>> tableColumnWidths;
    std::optional<float> tableSelectionWidth;
    std::optional<float> flexBasisPercent;
    std::optional<int> flexOrder;
    bool flexBasisSpecified = false;
};

struct StyleRule {
    std::string selector;
    StyleBox box;
    // CSS declarations retain their source values so var(--token) references
    // can be resolved again when application theme tokens change at runtime.
    // Programmatically constructed rules may continue to populate `box` only.
    std::vector<std::pair<std::string, std::string>> declarations;
    int order = 0;
    std::shared_ptr<const ContainerQuery> query;
};

struct StyleNode {
    std::string tag;
    std::vector<std::string> classes;
    StylePseudoMask state = StyleStateNone;
};

class ONEUI_API StyleSheet {
public:
    void addRule(StyleRule rule);
    // Optional palette for transient surfaces (dialogs/popups and select menus).
    // Weak reference: the application retains the variant, avoiding sheet cycles.
    void setTransientStyleSheet(const std::shared_ptr<StyleSheet>& sheet) { transient_ = sheet; }
    std::shared_ptr<StyleSheet> transientStyleSheet() const { return transient_.lock(); }
    void setCustomProperty(std::string name, std::string value);
    std::optional<std::string> customProperty(const std::string& name) const;
    bool addRulesFromCss(const std::string& css, std::string* error = nullptr);
    StyleBox resolve(const StyleNode& node) const;
    StyleLayout resolveLayout(const StyleNode& node) const;
    const std::vector<StyleRule>& rules() const;
    const std::map<std::string, std::string>& customProperties() const;
    std::size_t version() const;
    StyleSheet forContainers(const std::map<std::string,Size>& sizes) const;

private:
    std::vector<const StyleRule*> matchingRules(const StyleNode& node) const;
    struct CompiledSelector {
        StyleNode node;
        bool valid = false;
        int specificity = 0;
    };
    std::weak_ptr<StyleSheet> transient_;
    std::vector<StyleRule> rules_;
    // Selector syntax is independent of theme tokens and node pseudo-state.
    // Compile it once, instead of allocating strings for every cold resolve.
    std::vector<CompiledSelector> selectors_;
    std::map<std::string, std::string> customProperties_;
    mutable std::unordered_map<std::string, StyleBox> resolveCache_;
    // Declarative mounts re-resolve base layouts on every flush; cache them
    // under the same key and invalidation as resolveCache_.
    mutable std::unordered_map<std::string, StyleLayout> layoutCache_;
    std::size_t version_ = 0;
};

ONEUI_API StylePseudoMask parseStylePseudoState(const std::string& pseudo);
ONEUI_API std::optional<Color> parseStyleColor(const std::string& value);
ONEUI_API bool selectorMatches(const std::string& selector, const StyleNode& node);
ONEUI_API int selectorSpecificity(const std::string& selector);
ONEUI_API StyleBox mergeStyleBox(StyleBox base, const StyleBox& overlay);
// Bake background multipliers into color/gradient alpha, clearing the
// multipliers so adapters, caches and repeated painting cannot apply them twice.
ONEUI_API StyleBox resolveBackgroundOpacity(StyleBox box);
ONEUI_API Rect stylePaintBounds(Rect rect, const StyleBox& box);
ONEUI_API Rect styleContentRect(Rect rect, const StyleBox& box);
ONEUI_API void paintStyleBox(Canvas& canvas, Rect rect, const StyleBox& box);

} // namespace oneui

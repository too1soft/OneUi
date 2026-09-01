#pragma once

#include "oneui/animation.h"
#include "oneui/color.h"
#include "oneui/geometry.h"

#include <optional>
#include <vector>

namespace oneui {

enum class ControlState {
    Default,
    Hovered,
    Pressed,
    Disabled,
    ReadOnly,
    FocusVisible,
    Selected
};

struct FocusRingStyle {
    Color color{96, 165, 250};
    float width = 2.0f;
    float offset = 3.0f;
    float radius = 8.0f;
    bool visible = true;
};

struct FocusRingStyleOverride {
    std::optional<Color> color;
    std::optional<float> width;
    std::optional<float> offset;
    std::optional<float> radius;
    std::optional<bool> visible;
};

struct ControlShadowStyle {
    Color color{0, 0, 0, 0};
    Point offset{};
    float blurRadius = 0.0f;
    float spreadRadius = 0.0f;
    bool inset = false;
};

struct ButtonStyle {
    Color background{255, 255, 255};
    Color foreground{32, 33, 36};
    Color border{216, 219, 224};
    float borderWidth = 1.0f;
    float radius = 6.0f;
    float fontSize = 14.0f;
    int fontWeight = 400;
    FocusRingStyle focusRing{};
    TransitionSpec transition{};
    std::vector<ControlShadowStyle> shadows;
};

struct ButtonStateStyleOverride {
    std::optional<Color> background;
    std::optional<Color> foreground;
    std::optional<Color> border;
    std::optional<float> borderWidth;
    std::optional<float> radius;
    std::optional<float> fontSize;
    std::optional<int> fontWeight;
    std::optional<FocusRingStyleOverride> focusRing;
    std::optional<TransitionSpec> transition;
    std::optional<std::vector<ControlShadowStyle>> shadows;
};

struct ButtonStyleOverride {
    std::optional<ButtonStateStyleOverride> normal;
    std::optional<ButtonStateStyleOverride> hovered;
    std::optional<ButtonStateStyleOverride> pressed;
    std::optional<ButtonStateStyleOverride> disabled;
    std::optional<ButtonStateStyleOverride> selected;
    std::optional<ButtonStateStyleOverride> focusVisible;
};

struct CheckboxStyle {
    Color boxBackground{255, 255, 255};
    Color boxBorder{216, 219, 224};
    Color checkColor{255, 255, 255};
    Color labelColor{97, 103, 114};
    float borderWidth = 1.0f;
    float radius = 4.0f;
    FocusRingStyle focusRing{};
};

struct CheckboxStateStyleOverride {
    std::optional<Color> boxBackground;
    std::optional<Color> boxBorder;
    std::optional<Color> checkColor;
    std::optional<Color> labelColor;
    std::optional<float> borderWidth;
    std::optional<float> radius;
    std::optional<FocusRingStyleOverride> focusRing;
};

struct CheckboxStyleOverride {
    std::optional<CheckboxStateStyleOverride> normal;
    std::optional<CheckboxStateStyleOverride> hovered;
    std::optional<CheckboxStateStyleOverride> pressed;
    std::optional<CheckboxStateStyleOverride> disabled;
    std::optional<CheckboxStateStyleOverride> selected;
    std::optional<CheckboxStateStyleOverride> focusVisible;
};

struct TextFieldStyle {
    Color background{255, 255, 255};
    Color foreground{25, 28, 33};
    Color placeholderForeground{140, 146, 156};
    Color border{211, 216, 224};
    Color selectionBackground{191, 219, 254};
    Color caretColor{37, 99, 235};
    float borderWidth = 1.0f;
    float radius = 6.0f;
    Insets padding{0.0f, 12.0f};
    FocusRingStyle focusRing{};
    TransitionSpec transition{};
    std::vector<ControlShadowStyle> shadows;
};

struct TextFieldStateStyleOverride {
    std::optional<Color> background;
    std::optional<Color> foreground;
    std::optional<Color> placeholderForeground;
    std::optional<Color> border;
    std::optional<Color> selectionBackground;
    std::optional<Color> caretColor;
    std::optional<float> borderWidth;
    std::optional<float> radius;
    std::optional<Insets> padding;
    std::optional<FocusRingStyleOverride> focusRing;
    std::optional<TransitionSpec> transition;
    std::optional<std::vector<ControlShadowStyle>> shadows;
};

struct TextFieldStyleOverride {
    std::optional<TextFieldStateStyleOverride> normal;
    std::optional<TextFieldStateStyleOverride> hovered;
    std::optional<TextFieldStateStyleOverride> disabled;
    std::optional<TextFieldStateStyleOverride> readOnly;
    std::optional<TextFieldStateStyleOverride> focusVisible;
};

struct SwitchStyle {
    Color trackBackground{203, 213, 225};
    Color thumbBackground{255, 255, 255};
    Color labelColor{97, 103, 114};
    Color border{0, 0, 0, 0};
    float borderWidth = 0.0f;
    float radius = 12.0f;
    FocusRingStyle focusRing{};
};

struct SwitchStateStyleOverride {
    std::optional<Color> trackBackground;
    std::optional<Color> thumbBackground;
    std::optional<Color> labelColor;
    std::optional<Color> border;
    std::optional<float> borderWidth;
    std::optional<float> radius;
    std::optional<FocusRingStyleOverride> focusRing;
};

struct SwitchStyleOverride {
    std::optional<SwitchStateStyleOverride> normal;
    std::optional<SwitchStateStyleOverride> hovered;
    std::optional<SwitchStateStyleOverride> pressed;
    std::optional<SwitchStateStyleOverride> disabled;
    std::optional<SwitchStateStyleOverride> selected;
    std::optional<SwitchStateStyleOverride> focusVisible;
};

struct SliderStyle {
    Color trackBackground{226, 232, 240};
    Color trackFill{37, 99, 235};
    Color thumbBackground{37, 99, 235};
    Color thumbBorder{37, 99, 235};
    float trackHeight = 4.0f;
    float thumbSize = 16.0f;
    float thumbBorderWidth = 0.0f;
    FocusRingStyle focusRing{};
};

struct SliderStateStyleOverride {
    std::optional<Color> trackBackground;
    std::optional<Color> trackFill;
    std::optional<Color> thumbBackground;
    std::optional<Color> thumbBorder;
    std::optional<float> trackHeight;
    std::optional<float> thumbSize;
    std::optional<float> thumbBorderWidth;
    std::optional<FocusRingStyleOverride> focusRing;
};

struct SliderStyleOverride {
    std::optional<SliderStateStyleOverride> normal;
    std::optional<SliderStateStyleOverride> hovered;
    std::optional<SliderStateStyleOverride> pressed;
    std::optional<SliderStateStyleOverride> disabled;
    std::optional<SliderStateStyleOverride> focusVisible;
};

struct RadioGroupStyle {
    Color itemBackground{0, 0, 0, 0};
    Color indicatorBackground{255, 255, 255};
    Color indicatorBorder{71, 85, 105};
    Color indicatorFill{37, 99, 235};
    Color labelColor{97, 103, 114};
    Color selectedLabelColor{97, 103, 114};
    float indicatorSize = 16.0f;
    float indicatorDotSize = 8.0f;
    float indicatorBorderWidth = 1.5f;
    float indicatorRadius = 8.0f;
    float itemRadius = 6.0f;
    float indicatorInset = 8.0f;
    float labelGap = 10.0f;
    FocusRingStyle focusRing{};
};

struct RadioGroupStateStyleOverride {
    std::optional<Color> itemBackground;
    std::optional<Color> indicatorBackground;
    std::optional<Color> indicatorBorder;
    std::optional<Color> indicatorFill;
    std::optional<Color> labelColor;
    std::optional<Color> selectedLabelColor;
    std::optional<float> indicatorSize;
    std::optional<float> indicatorDotSize;
    std::optional<float> indicatorBorderWidth;
    std::optional<float> indicatorRadius;
    std::optional<float> itemRadius;
    std::optional<float> indicatorInset;
    std::optional<float> labelGap;
    std::optional<FocusRingStyleOverride> focusRing;
};

struct RadioGroupStyleOverride {
    std::optional<RadioGroupStateStyleOverride> normal;
    std::optional<RadioGroupStateStyleOverride> hovered;
    std::optional<RadioGroupStateStyleOverride> pressed;
    std::optional<RadioGroupStateStyleOverride> disabled;
    std::optional<RadioGroupStateStyleOverride> selected;
    std::optional<RadioGroupStateStyleOverride> focusVisible;
};

struct SelectStyle {
    Color background{255, 255, 255};
    Color foreground{25, 28, 33};
    Color border{211, 216, 224};
    Color arrowColor{97, 103, 114};
    Color popupBackground{255, 255, 255};
    Color popupBorder{71, 85, 105};
    Color popupShadow{148, 163, 184, 60};
    Color optionBackground{0, 0, 0, 0};
    Color optionForeground{25, 28, 33};
    Color selectedOptionBackground{219, 234, 254};
    Color selectedOptionForeground{37, 99, 235};
    float borderWidth = 1.0f;
    float radius = 6.0f;
    float popupRadius = 6.0f;
    float optionRadius = 6.0f;
    float popupOffset = 4.0f;
    float popupShadowOffset = 2.0f;
    Insets padding{0.0f, 12.0f};
    Insets optionInset{2.0f, 4.0f};
    FocusRingStyle focusRing{};
};

struct SelectStateStyleOverride {
    std::optional<Color> background;
    std::optional<Color> foreground;
    std::optional<Color> border;
    std::optional<Color> arrowColor;
    std::optional<Color> popupBackground;
    std::optional<Color> popupBorder;
    std::optional<Color> popupShadow;
    std::optional<Color> optionBackground;
    std::optional<Color> optionForeground;
    std::optional<Color> selectedOptionBackground;
    std::optional<Color> selectedOptionForeground;
    std::optional<float> borderWidth;
    std::optional<float> radius;
    std::optional<float> popupRadius;
    std::optional<float> optionRadius;
    std::optional<float> popupOffset;
    std::optional<float> popupShadowOffset;
    std::optional<Insets> padding;
    std::optional<Insets> optionInset;
    std::optional<FocusRingStyleOverride> focusRing;
};

struct SelectStyleOverride {
    std::optional<SelectStateStyleOverride> normal;
    std::optional<SelectStateStyleOverride> hovered;
    std::optional<SelectStateStyleOverride> pressed;
    std::optional<SelectStateStyleOverride> disabled;
    std::optional<SelectStateStyleOverride> selected;
    std::optional<SelectStateStyleOverride> focusVisible;
};

struct TabsStyle {
    Color background{241, 245, 249};
    Color border{226, 232, 240};
    Color itemBackground{0, 0, 0, 0};
    Color itemForeground{97, 103, 114};
    Color selectedItemBackground{255, 255, 255};
    Color selectedItemForeground{37, 99, 235};
    Color selectedItemBorder{226, 232, 240};
    float borderWidth = 1.0f;
    float radius = 6.0f;
    float itemRadius = 6.0f;
    float itemBorderWidth = 1.0f;
    float fontSize = 14.0f;
    int fontWeight = 400;
    Insets itemInset{2.0f};
    FocusRingStyle focusRing{};
};

struct TabsStateStyleOverride {
    std::optional<Color> background;
    std::optional<Color> border;
    std::optional<Color> itemBackground;
    std::optional<Color> itemForeground;
    std::optional<Color> selectedItemBackground;
    std::optional<Color> selectedItemForeground;
    std::optional<Color> selectedItemBorder;
    std::optional<float> borderWidth;
    std::optional<float> radius;
    std::optional<float> itemRadius;
    std::optional<float> itemBorderWidth;
    std::optional<float> fontSize;
    std::optional<int> fontWeight;
    std::optional<Insets> itemInset;
    std::optional<FocusRingStyleOverride> focusRing;
};

struct TabsStyleOverride {
    std::optional<TabsStateStyleOverride> normal;
    std::optional<TabsStateStyleOverride> hovered;
    std::optional<TabsStateStyleOverride> pressed;
    std::optional<TabsStateStyleOverride> disabled;
    std::optional<TabsStateStyleOverride> selected;
    std::optional<TabsStateStyleOverride> focusVisible;
};

struct ListStyle {
    Color background{255, 255, 255};
    Color border{211, 216, 224};
    Color separator{211, 216, 224};
    Color rowBackground{0, 0, 0, 0};
    Color titleColor{25, 28, 33};
    Color detailColor{97, 103, 114};
    Color selectedRowBackground{219, 234, 254};
    Color selectedTitleColor{37, 99, 235};
    Color selectedDetailColor{97, 103, 114};
    float borderWidth = 1.0f;
    float radius = 6.0f;
    float rowRadius = 4.0f;
    Insets rowInset{3.0f, 4.0f};
    float textInset = 12.0f;
    float titleOffsetY = 7.0f;
    float detailOffsetY = 27.0f;
    float titleFontSize = 14.0f;
    float detailFontSize = 12.0f;
    int titleFontWeight = 400;
    int detailFontWeight = 400;
    Color scrollbarColor{0, 0, 0, 0};
    float scrollbarWidth = 4.0f;
    FocusRingStyle focusRing{};
};

struct ListStateStyleOverride {
    std::optional<Color> background;
    std::optional<Color> border;
    std::optional<Color> separator;
    std::optional<Color> rowBackground;
    std::optional<Color> titleColor;
    std::optional<Color> detailColor;
    std::optional<Color> selectedRowBackground;
    std::optional<Color> selectedTitleColor;
    std::optional<Color> selectedDetailColor;
    std::optional<float> borderWidth;
    std::optional<float> radius;
    std::optional<float> rowRadius;
    std::optional<Insets> rowInset;
    std::optional<float> textInset;
    std::optional<float> titleOffsetY;
    std::optional<float> detailOffsetY;
    std::optional<float> titleFontSize;
    std::optional<float> detailFontSize;
    std::optional<int> titleFontWeight;
    std::optional<int> detailFontWeight;
    std::optional<Color> scrollbarColor;
    std::optional<float> scrollbarWidth;
    std::optional<FocusRingStyleOverride> focusRing;
};

struct ListStyleOverride {
    std::optional<ListStateStyleOverride> normal;
    std::optional<ListStateStyleOverride> hovered;
    std::optional<ListStateStyleOverride> pressed;
    std::optional<ListStateStyleOverride> disabled;
    std::optional<ListStateStyleOverride> selected;
    std::optional<ListStateStyleOverride> focusVisible;
};

// Tree navigation follows the same visual contract as a selectable list while
// adding hierarchy through the TreeView control itself.
using TreeViewStyle = ListStyle;
using TreeViewStateStyleOverride = ListStateStyleOverride;
using TreeViewStyleOverride = ListStyleOverride;

struct TableStyle {
    Color background{255, 255, 255};
    Color border{211, 216, 224};
    Color headerBackground{241, 245, 249};
    Color headerForeground{97, 103, 114};
    Color cellForeground{25, 28, 33};
    Color gridLine{211, 216, 224};
    Color rowHovered{241, 245, 249};
    Color rowPressed{226, 232, 240};
    Color rowSelected{224, 231, 255};
    Color scrollbarColor{148, 163, 184};
    float borderWidth = 1.0f;
    float radius = 6.0f;
    float headerHeight = 30.0f;
    float scrollbarWidth = 4.0f;
    Insets cellPadding{0.0f, 10.0f};
};

struct TableStyleOverride {
    std::optional<Color> background;
    std::optional<Color> border;
    std::optional<Color> headerBackground;
    std::optional<Color> headerForeground;
    std::optional<Color> cellForeground;
    std::optional<Color> gridLine;
    std::optional<Color> rowHovered;
    std::optional<Color> rowPressed;
    std::optional<Color> rowSelected;
    std::optional<Color> scrollbarColor;
    std::optional<float> borderWidth;
    std::optional<float> radius;
    std::optional<float> headerHeight;
    std::optional<float> scrollbarWidth;
    std::optional<Insets> cellPadding;
};

struct BadgeStyle {
    Color background{241, 245, 249};
    Color foreground{97, 103, 114};
    Color border{211, 216, 224};
    float borderWidth = 1.0f;
    float radius = 12.0f;
    Insets padding{0.0f, 8.0f};
    float fontSize = 12.0f;
};

struct BadgeStyleOverride {
    std::optional<Color> background;
    std::optional<Color> foreground;
    std::optional<Color> border;
    std::optional<float> borderWidth;
    std::optional<float> radius;
    std::optional<Insets> padding;
    std::optional<float> fontSize;
};

struct ProgressBarStyle {
    Color trackBackground{226, 232, 240};
    Color fill{37, 99, 235};
    Color disabledFill{148, 163, 184};
    float radius = 5.0f;
};

struct ProgressBarStyleOverride {
    std::optional<Color> trackBackground;
    std::optional<Color> fill;
    std::optional<Color> disabledFill;
    std::optional<float> radius;
};

struct SeparatorStyle {
    Color color{211, 216, 224};
    float thickness = 1.0f;
};

struct SeparatorStyleOverride {
    std::optional<Color> color;
    std::optional<float> thickness;
};

struct ValidationMessageStyle {
    Color helperColor{97, 103, 114};
    Color errorColor{220, 38, 38};
    float fontSize = 12.0f;
    float lineHeight = 18.0f;
};

struct ValidationMessageStyleOverride {
    std::optional<Color> helperColor;
    std::optional<Color> errorColor;
    std::optional<float> fontSize;
    std::optional<float> lineHeight;
};

struct FormFieldStyle {
    Color labelColor{25, 28, 33};
    Color helperColor{97, 103, 114};
    Color errorColor{220, 38, 38};
    Color requiredMarkerColor{220, 38, 38};
    Insets padding{0.0f};
    float labelFontSize = 13.0f;
    float messageFontSize = 12.0f;
    float labelLineHeight = 20.0f;
    float messageLineHeight = 18.0f;
    float labelGap = 6.0f;
    float controlGap = 6.0f;
};

struct FormFieldStyleOverride {
    std::optional<Color> labelColor;
    std::optional<Color> helperColor;
    std::optional<Color> errorColor;
    std::optional<Color> requiredMarkerColor;
    std::optional<Insets> padding;
    std::optional<float> labelFontSize;
    std::optional<float> messageFontSize;
    std::optional<float> labelLineHeight;
    std::optional<float> messageLineHeight;
    std::optional<float> labelGap;
    std::optional<float> controlGap;
};

struct PopupStyle {
    Color background{255, 255, 255};
    Color foreground{25, 28, 33};
    Color border{211, 216, 224};
    float borderWidth = 1.0f;
    float radius = 8.0f;
    Insets padding{8.0f};
    float offset = 6.0f;
    float elevation = 1.0f;
    int layer = 100;
};

struct PopupStyleOverride {
    std::optional<Color> background;
    std::optional<Color> foreground;
    std::optional<Color> border;
    std::optional<float> borderWidth;
    std::optional<float> radius;
    std::optional<Insets> padding;
    std::optional<float> offset;
    std::optional<float> elevation;
    std::optional<int> layer;
};

struct Theme {
    Color appBackground{242, 244, 247};
    Color surface{255, 255, 255};
    Color surfaceMuted{248, 250, 252};
    Color text{25, 28, 33};
    Color textMuted{97, 103, 114};
    Color textSubtle{140, 146, 156};
    Color border{211, 216, 224};
    Color borderStrong{148, 163, 184};
    Color primary{37, 99, 235};
    Color primaryHover{29, 78, 216};
    Color primaryPressed{30, 64, 175};
    Color primarySoft{219, 234, 254};
    Color focusOutline{96, 165, 250};
    Color success{22, 163, 74};
    Color successSoft{220, 252, 231};
    Color warning{217, 119, 6};
    Color warningSoft{254, 243, 199};

    float radiusSm = 4.0f;
    float radiusMd = 6.0f;
    float radiusLg = 8.0f;
    float focusOutlineWidth = 2.0f;
    float focusOutlineOffset = 3.0f;
    float fontSm = 12.0f;
    float fontMd = 13.0f;
    float fontLg = 17.0f;

    FormFieldStyle formField{
        text,
        textMuted,
        Color{220, 38, 38},
        Color{220, 38, 38},
        Insets{0.0f},
        fontMd,
        fontSm,
        20.0f,
        18.0f,
        6.0f,
        6.0f
    };
    PopupStyle popup{
        surface,
        text,
        border,
        1.0f,
        radiusLg,
        Insets{8.0f},
        6.0f,
        1.0f,
        100
    };

    // Appended semantic tokens preserve existing aggregate initialization while
    // giving native styles stable CSS-like names for future component defaults.
    Color error{220, 38, 38};
    Color errorSoft{254, 226, 226};
    Color disabledBackground{238, 241, 245};
    Color disabledForeground{148, 163, 184};
    Color disabledBorder{226, 232, 240};
    Color hoverBackground{242, 244, 247};
    Color pressedBackground{232, 236, 242};
    Color selectedBackground{primarySoft};
    Color selectedForeground{primary};
    Color shadowColor{15, 23, 42, 38};

    float paddingSm = 4.0f;
    float paddingMd = 8.0f;
    float paddingLg = 12.0f;
    float gapXs = 2.0f;
    float gapSm = 6.0f;
    float gapMd = 8.0f;
    float gapLg = 12.0f;
    float shadowSm = 1.0f;
    float shadowMd = 2.0f;

    int layerBase = 0;
    int layerOverlay = 50;
    int layerPopup = 100;

    Color hoverForeground{text};
    Color hoverBorder{border};
    Color pressedForeground{text};
    Color pressedBorder{borderStrong};
    Color selectedBorder{primary};
};

inline const Theme& theme() {
    static const Theme value{};
    return value;
}

} // namespace oneui

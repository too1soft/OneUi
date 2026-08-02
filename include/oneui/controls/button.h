#pragma once

#include "oneui/export.h"
#include "oneui/animation.h"
#include "oneui/icon.h"
#include "oneui/style.h"
#include "oneui/widget.h"

#include <functional>
#include <optional>
#include <string>

namespace oneui {

enum class ButtonVariant {
    Primary,
    Secondary
};

class ONEUI_API Button final : public Widget {
public:
    explicit Button(std::wstring text);

    void setText(std::wstring text);
    const std::wstring& text() const;
    void bindText(State<std::wstring>& state);
    void setVariant(ButtonVariant variant);
    void setIcon(IconSymbol symbol);
    void clearIcon();
    void setContentAlign(TextAlign align);
    void setTrailingText(std::wstring text);
    void setStyleOverride(ButtonStyleOverride style);
    void clearStyleOverride();
    void setDisabled(bool disabled) override;
    void setOnClick(std::function<void()> callback);

    void paint(Canvas& canvas) override;
    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    bool onKeyDown(const KeyEvent& event) override;
    CursorKind cursor(Point point) const override;
    bool isFocusable() const override;
    bool tickAnimations(double nowMs) override;
    AccessibilityInfo accessibilityInfo() const override;

private:
    void click();
    ButtonStyle resolvedStyle() const;
    ButtonStyle visualStyle(ButtonStyle target) const;
    void beginVisualTransition(ButtonStyle from, ButtonStyle target);
    bool hasInteractionState() const override;
    void resetInteractionState() override;

    std::wstring text_;
    Binding<std::wstring> textBinding_;
    ButtonVariant variant_ = ButtonVariant::Primary;
    std::optional<IconSymbol> icon_;
    TextAlign contentAlign_ = TextAlign::Center;
    std::wstring trailingText_;
    std::optional<ButtonStyleOverride> styleOverride_;
    bool hovered_ = false;
    bool pressed_ = false;
    bool visualInitialized_ = false;
    ColorTransition backgroundTransition_;
    ColorTransition foregroundTransition_;
    ColorTransition borderTransition_;
    std::function<void()> onClick_;
};

} // namespace oneui

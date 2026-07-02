#pragma once

#include "oneui/export.h"
#include "oneui/icon.h"
#include "oneui/style_sheet.h"
#include "oneui/style_transition.h"
#include "oneui/widget.h"

#include <functional>
#include <memory>

namespace oneui {

class ONEUI_API IconButton final : public Widget {
public:
    explicit IconButton(IconSymbol symbol = IconSymbol::Monitor);

    void setSymbol(IconSymbol symbol);
    IconSymbol symbol() const;
    void setStyleSheet(std::shared_ptr<StyleSheet> sheet, StyleNode node);
    void setOnClick(std::function<void()> callback);

    void paint(Canvas& canvas) override;
    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    CursorKind cursor(Point point) const override;
    bool onFocusChanged(bool focused) override;
    bool isFocusable() const override;
    bool tickAnimations(double nowMs) override;
    void setDisabled(bool disabled) override;

private:
    StyleBox resolvedStyle() const;
    StyleBox visualStyle(StyleBox target) const;
    void beginVisualTransition(StyleBox from, StyleBox target);
    void updateAccessibility();
    bool hasInteractionState() const override;
    void resetInteractionState() override;

    IconSymbol symbol_;
    std::shared_ptr<StyleSheet> styleSheet_;
    StyleNode styleNode_{"button", {"icon-button"}, StyleStateNone};
    std::function<void()> onClick_;
    bool hovered_ = false;
    bool pressed_ = false;
    StyleBoxTransition visualTransition_;
};

} // namespace oneui

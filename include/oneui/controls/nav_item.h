#pragma once

#include "oneui/icon.h"
#include "oneui/layout/sidebar_nav_bridge.h"
#include "oneui/style_sheet.h"
#include "oneui/style_transition.h"
#include "oneui/widget.h"

#include <functional>
#include <map>
#include <memory>
#include <string>

namespace oneui {

class ONEUI_API NavItem final : public Widget {
public:
    explicit NavItem(std::wstring text = {}, IconSymbol symbol = IconSymbol::RemoteAssist);

    void setText(std::wstring text);
    void setSymbol(IconSymbol symbol);
    void setSelected(bool selected);
    bool selected() const;
    void setStyleSheet(std::shared_ptr<StyleSheet> sheet);
    void setOnClick(std::function<void()> callback);

    void paint(Canvas& canvas) override;
    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    bool onFocusChanged(bool focused) override;
    bool isFocusable() const override;
    bool tickAnimations(double nowMs) override;
    void setDisabled(bool disabled) override;

private:
    SidebarNavItemBridgeLayout layout() const;
    StyleBox resolvedItemStyle() const;
    StyleBox cachedItemStyle(StylePseudoMask state) const;
    StylePseudoMask currentStateMask() const;
    void clearStyleCache() const;
    StyleBox visualItemStyle(StyleBox target) const;
    void beginVisualTransition(StyleBox from, StyleBox target);
    void updateAccessibility();
    bool hasInteractionState() const override;
    void resetInteractionState() override;

    std::wstring text_;
    IconSymbol symbol_ = IconSymbol::RemoteAssist;
    std::shared_ptr<StyleSheet> styleSheet_;
    std::function<void()> onClick_;
    bool selected_ = false;
    bool hovered_ = false;
    bool pressed_ = false;
    StyleBoxTransition visualTransition_;
    mutable std::map<int, StyleBox> styleCache_;
    mutable std::size_t styleCacheVersion_ = 0;
};

} // namespace oneui

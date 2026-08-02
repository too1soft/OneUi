#pragma once

#include "oneui/export.h"
#include "oneui/icon.h"
#include "oneui/style_sheet.h"
#include "oneui/widget.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace oneui {

/// A centered content state for empty, loading, no-result, and error views.
class ONEUI_API StateView final : public Widget {
public:
    StateView(std::wstring title = {}, std::wstring message = {});

    void setTitle(std::wstring title);
    void setMessage(std::wstring message);
    void setIcon(IconSymbol symbol);
    void setAction(std::wstring text);
    void setOnAction(std::function<void()> callback);
    void setStyleSheet(std::shared_ptr<StyleSheet> sheet, StyleNode node);

    void paint(Canvas& canvas) override;
    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    bool isFocusable() const override;
    bool onFocusChanged(bool focused) override;

private:
    struct Layout {
        Rect icon;
        Rect title;
        Rect message;
        Rect action;
    };

    Layout layout() const;
    StyleBox resolvedStyle() const;
    StyleBox resolvedActionStyle() const;
    StyleNode childStyleNode(const std::vector<std::string>& classes, StylePseudoMask state) const;
    bool actionAt(Point point) const;
    bool hasInteractionState() const override;
    void resetInteractionState() override;

    std::wstring title_;
    std::wstring message_;
    IconSymbol icon_ = IconSymbol::Server;
    std::wstring action_;
    std::function<void()> onAction_;
    std::shared_ptr<StyleSheet> styleSheet_;
    StyleNode styleNode_{"state-view", {"state-view"}, StyleStateNone};
    bool hovered_ = false;
    bool pressed_ = false;
};

} // namespace oneui

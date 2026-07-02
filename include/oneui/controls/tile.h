#pragma once

#include "oneui/icon.h"
#include "oneui/style_sheet.h"
#include "oneui/widget.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace oneui {

class ONEUI_API Tile final : public Widget {
public:
    Tile(std::wstring title = {}, std::wstring subtitle = {});

    void setTitle(std::wstring title);
    void setSubtitle(std::wstring subtitle);
    void setLeadingSymbol(IconSymbol symbol);
    void clearLeadingSymbol();
    void setTrailingSymbol(IconSymbol symbol);
    void clearTrailingSymbol();
    void setStyleSheet(std::shared_ptr<StyleSheet> sheet, StyleNode node);
    void setOnClick(std::function<void()> callback);

    void paint(Canvas& canvas) override;
    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    bool onFocusChanged(bool focused) override;
    bool isFocusable() const override;
    void setDisabled(bool disabled) override;

private:
    StyleBox resolvedStyle() const;
    void updateAccessibility();
    bool hasInteractionState() const override;
    void resetInteractionState() override;

    std::wstring title_;
    std::wstring subtitle_;
    std::optional<IconSymbol> leadingSymbol_;
    std::optional<IconSymbol> trailingSymbol_;
    std::shared_ptr<StyleSheet> styleSheet_;
    StyleNode styleNode_{"tile", {"tile"}, StyleStateNone};
    std::function<void()> onClick_;
    bool hovered_ = false;
    bool pressed_ = false;
};

} // namespace oneui

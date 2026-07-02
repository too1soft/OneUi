#pragma once

#include "oneui/export.h"
#include "oneui/style_sheet.h"
#include "oneui/widget.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace oneui {

class ONEUI_API StatusStrip final : public Widget {
public:
    StatusStrip(std::wstring title = {}, std::wstring message = {});

    void setTitle(std::wstring title);
    void setMessage(std::wstring message);
    void setPrimaryAction(std::wstring text);
    void setSecondaryAction(std::wstring text);
    void setOnPrimaryAction(std::function<void()> callback);
    void setOnSecondaryAction(std::function<void()> callback);
    void setStyleSheet(std::shared_ptr<StyleSheet> sheet, StyleNode node);

    void paint(Canvas& canvas) override;
    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    bool isFocusable() const override;
    bool onFocusChanged(bool focused) override;

private:
    enum class Action { None, Primary, Secondary };

    struct Layout {
        Rect icon;
        Rect title;
        Rect message;
        Rect primary;
        Rect secondary;
    };

    Layout layout() const;
    StyleBox resolvedStyle() const;
    StyleBox resolvedActionStyle(Action action) const;
    StyleNode childStyleNode(const std::vector<std::string>& classes, StylePseudoMask state) const;
    Action actionAt(Point point) const;
    bool hasInteractionState() const override;
    void resetInteractionState() override;

    std::wstring title_;
    std::wstring message_;
    std::wstring primaryAction_;
    std::wstring secondaryAction_;
    std::function<void()> onPrimaryAction_;
    std::function<void()> onSecondaryAction_;
    std::shared_ptr<StyleSheet> styleSheet_;
    StyleNode styleNode_{"status-strip", {"status-strip"}, StyleStateNone};
    Action hoveredAction_ = Action::None;
    Action pressedAction_ = Action::None;
};

} // namespace oneui

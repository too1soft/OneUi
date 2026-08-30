#pragma once

#include "oneui/export.h"
#include "oneui/reactive.h"
#include "oneui/style.h"
#include "oneui/widget.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace oneui {

class ONEUI_API Select final : public Widget {
public:
    Select();

    void setItems(std::vector<std::wstring> items);
    void setSelectedIndex(int index);
    int selectedIndex() const;
    void bindSelectedIndex(State<int>& state);
    void setStyleOverride(SelectStyleOverride style);
    void clearStyleOverride();
    void setOnChanged(std::function<void(int)> callback);

    void paint(Canvas& canvas) override;
    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    bool onKeyDown(const KeyEvent& event) override;
    bool onFocusChanged(bool focused) override;
    bool isFocusable() const override;
    bool hitTest(Point point) const override;
    bool paintsAboveSiblings() const override;
    AccessibilityInfo accessibilityInfo() const override;

private:
    void assignSelectedIndex(int index);
    std::wstring selectedText() const;
    SelectStyle resolvedFieldStyle() const;
    SelectStyle resolvedOptionStyle(int index) const;
    bool hasInteractionState() const override;
    void resetInteractionState() override;
    // Local inline popup geometry resolved against the stable paint viewport.
    float popupRowHeight() const;
    Rect popupSurfaceRect() const;
    Rect popupOptionRect(int index) const;
    int hitPopupOptionIndex(Point point) const;
    int effectiveSelectedIndex() const;
    enum class PopupLightDismissReason {
        OutsidePointer,
        EscapeKey,
        FocusLost,
        Unavailable,
        Reset
    };
    struct LightDismissModel {
        bool open = false;
        bool fieldPressed = false;
        int hoveredIndex = -1;
        int pressedIndex = -1;
    };
    void openPopupSurface();
    void closePopupSurface(PopupLightDismissReason reason = PopupLightDismissReason::Reset);

    std::vector<std::wstring> items_;
    int selectedIndex_ = 0;
    bool hovered_ = false;
    LightDismissModel popup_;
    Binding<int> selectedBinding_;
    std::optional<SelectStyleOverride> styleOverride_;
    std::optional<Rect> popupViewport_;
    std::function<void(int)> onChanged_;
};

} // namespace oneui

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

class ONEUI_API RadioGroup final : public Widget {
public:
    RadioGroup();

    enum class Orientation {
        Vertical,
        Horizontal
    };

    void setItems(std::vector<std::wstring> items);
    void setSelectedIndex(int index);
    int selectedIndex() const;
    void setOrientation(Orientation orientation);
    Orientation orientation() const;
    void bindSelectedIndex(State<int>& state);
    void setStyleOverride(RadioGroupStyleOverride style);
    void clearStyleOverride();
    void setOnChanged(std::function<void(int)> callback);

    void paint(Canvas& canvas) override;
    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    bool onKeyDown(const KeyEvent& event) override;
    bool isFocusable() const override;
    AccessibilityInfo accessibilityInfo() const override;

private:
    int hitIndex(Point point) const;
    Rect itemRect(int index) const;
    RadioGroupStyle resolvedStyle(int index) const;
    void assignSelectedIndex(int index);
    bool hasInteractionState() const override;
    void resetInteractionState() override;

    std::vector<std::wstring> items_;
    int selectedIndex_ = 0;
    Orientation orientation_ = Orientation::Vertical;
    int hoveredIndex_ = -1;
    int pressedIndex_ = -1;
    Binding<int> selectedBinding_;
    std::optional<RadioGroupStyleOverride> styleOverride_;
    std::function<void(int)> onChanged_;
};

} // namespace oneui

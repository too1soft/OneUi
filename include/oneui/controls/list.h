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

struct ListItem {
    std::wstring title;
    std::wstring detail;
};

class ONEUI_API List final : public Widget {
public:
    List();

    void setItems(std::vector<ListItem> items);
    void setSelectionRequired(bool required);
    bool selectionRequired() const;
    void setSelectedIndex(int index);
    int selectedIndex() const;
    void bindSelectedIndex(State<int>& state);
    void setStyleOverride(ListStyleOverride style);
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
    void assignSelectedIndex(int index);
    int effectiveSelectedIndex() const;
    int hitItemIndex(Point point) const;
    Rect itemRect(int index) const;
    ListStyle resolvedContainerStyle() const;
    ListStyle resolvedItemStyle(int index) const;
    float rowHeight() const;
    bool hasInteractionState() const override;
    void resetInteractionState() override;

    std::vector<ListItem> items_;
    bool selectionRequired_ = true;
    int selectedIndex_ = 0;
    int hoveredIndex_ = -1;
    int pressedIndex_ = -1;
    Binding<int> selectedBinding_;
    std::optional<ListStyleOverride> styleOverride_;
    std::function<void(int)> onChanged_;
};

} // namespace oneui

#pragma once

#include "oneui/animation.h"
#include "oneui/controls/list.h"
#include "oneui/export.h"

#include <functional>
#include <optional>
#include <vector>

namespace oneui {

/// Fixed-height, viewport-virtualized list for large simple data sets.
///
/// It owns data items rather than child widgets, so scrolling cost is bounded
/// by the visible rows instead of the total result count. Complex row editors
/// should use a dedicated virtualized presenter rather than this control.
class ONEUI_API VirtualList final : public Widget {
public:
    VirtualList();

    void setItems(std::vector<ListItem> items);
    void setSelectedIndex(int index);
    int selectedIndex() const;
    void setRowHeight(float height);
    float rowHeight() const;
    void setWheelStep(float step);
    void setScrollOffset(float offset);
    float scrollOffset() const;
    float maxScrollOffset() const;
    void setStyleOverride(ListStyleOverride style);
    void clearStyleOverride();
    void setOnChanged(std::function<void(int)> callback);

    void paint(Canvas& canvas) override;
    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    bool onMouseWheel(const MouseWheelEvent& event) override;
    bool onKeyDown(const KeyEvent& event) override;
    bool isFocusable() const override;
    AccessibilityInfo accessibilityInfo() const override;
    bool tickAnimations(double nowMs) override;

private:
    void assignSelectedIndex(int index);
    int effectiveSelectedIndex() const;
    int hitItemIndex(Point point) const;
    Rect itemRect(int index) const;
    Rect verticalThumbRect() const;
    void ensureSelectionVisible();
    ListStyle resolvedContainerStyle() const;
    ListStyle resolvedItemStyle(int index) const;
    bool hasInteractionState() const override;
    void resetInteractionState() override;

    std::vector<ListItem> items_;
    int selectedIndex_ = 0;
    int hoveredIndex_ = -1;
    int pressedIndex_ = -1;
    float rowHeight_ = 48.0f;
    float wheelStep_ = 48.0f;
    float scrollOffset_ = 0.0f;
    FloatTransition scrollTransition_;
    std::optional<ListStyleOverride> styleOverride_;
    std::function<void(int)> onChanged_;
};

} // namespace oneui

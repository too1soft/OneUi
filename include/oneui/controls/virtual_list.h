#pragma once

#include "oneui/animation.h"
#include "oneui/controls/list.h"
#include "oneui/export.h"
#include "oneui/selection_model.h"

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
    void setSelectionMode(SelectionMode mode);
    SelectionMode selectionMode() const;
    void setSelectedIndices(std::vector<int> indices);
    const std::vector<int>& selectedIndices() const;
    void setRowHeight(float height);
    float rowHeight() const;
    void setWheelStep(float step);
    void setScrollOffset(float offset);
    float scrollOffset() const;
    float maxScrollOffset() const;
    void setStyleOverride(ListStyleOverride style);
    void clearStyleOverride();
    void setOnChanged(std::function<void(int)> callback);
    void setOnSelectionChanged(std::function<void(const std::vector<int>&)> callback);
    void setOnActivated(std::function<void(int)> callback);
    void setOnEditRequested(std::function<void(int)> callback);
    void setOnContextMenuRequested(std::function<void(int, Point)> callback);
    void setReorderEnabled(bool enabled);
    bool reorderEnabled() const;
    void setOnReorderRequested(std::function<void(int, int)> callback);

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
    void notifySelectionChanged(const std::vector<int>& previousIndices, int previousSelectedIndex);
    void resetScrollMotion(float offset);
    bool advanceScrollMotion(double nowMs);
    int effectiveSelectedIndex() const;
    int hitItemIndex(Point point) const;
    Rect itemRect(int index) const;
    Rect verticalThumbRect(float width) const;
    void resetReorderState();
    void updateReorderTarget(Point point);
    void ensureSelectionVisible();
    ListStyle resolvedContainerStyle() const;
    ListStyle resolvedItemStyle(int index) const;
    bool hasInteractionState() const override;
    void resetInteractionState() override;

    std::vector<ListItem> items_;
    SelectionModel selection_;
    int hoveredIndex_ = -1;
    int pressedIndex_ = -1;
    int pressedClickCount_ = 1;
    bool reorderEnabled_ = false;
    bool reordering_ = false;
    Point reorderStartPoint_{};
    int reorderSourceIndex_ = -1;
    int reorderTargetIndex_ = -1;
    int reorderInsertionIndex_ = -1;
    float rowHeight_ = 48.0f;
    float wheelStep_ = 48.0f;
    float scrollOffset_ = 0.0f;
    SmoothScrollMotion scrollMotion_;
    double scrollTraceLastWheelMs_ = 0.0;
    double scrollTraceLastTickMs_ = 0.0;
    std::optional<ListStyleOverride> styleOverride_;
    std::function<void(int)> onChanged_;
    std::function<void(const std::vector<int>&)> onSelectionChanged_;
    std::function<void(int)> onActivated_;
    std::function<void(int)> onEditRequested_;
    std::function<void(int, Point)> onContextMenuRequested_;
    std::function<void(int, int)> onReorderRequested_;
};

} // namespace oneui

#pragma once

#include "oneui/animation.h"
#include "oneui/controls/list.h"
#include "oneui/export.h"
#include "oneui/input/item_drag.h"
#include "oneui/selection_model.h"

#include <functional>
#include <cstddef>
#include <optional>
#include <vector>

namespace oneui {

struct VirtualListItem {
    std::wstring title;
    std::wstring detail;
    std::wstring badge;
    std::wstring trailing;
    Color indicatorColor{0, 0, 0, 0};
    Color trailingColor{0, 0, 0, 0};
    bool indicatorVisible = false;
};

struct VirtualListRichMetrics {
    float indicatorSpace = 20.0f;
    float indicatorDiameter = 9.0f;
    float badgeHeight = 20.0f;
    float badgeRadius = 10.0f;
    float badgeHorizontalPadding = 14.0f;
    float titleBadgeGap = 7.0f;
    float trailingWidth = 58.0f;
    float trailingGap = 8.0f;
};

/// Fixed-height, viewport-virtualized list for large simple data sets.
///
/// It owns data items rather than child widgets, so scrolling cost is bounded
/// by the visible rows instead of the total result count. Complex row editors
/// should use a dedicated virtualized presenter rather than this control.
class ONEUI_API VirtualList final : public Widget {
public:
    VirtualList();

    void setItems(std::vector<ListItem> items);
    void setRichItems(std::vector<VirtualListItem> items);
    bool updateItem(std::size_t index, ListItem item);
    bool updateRichItem(std::size_t index, VirtualListItem item);
    void setSelectedIndex(int index);
    int selectedIndex() const;
    const std::vector<VirtualListItem>& items() const;
    Rect itemFrame(int index) const;
    void setSelectionMode(SelectionMode mode);
    SelectionMode selectionMode() const;
    void setSelectedIndices(std::vector<int> indices);
    const std::vector<int>& selectedIndices() const;
    void setRowHeight(float height);
    float rowHeight() const;
    void setRichMetrics(VirtualListRichMetrics metrics);
    const VirtualListRichMetrics& richMetrics() const;
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
    void setOnDeleteRequested(std::function<void(const std::vector<int>&)> callback);
    void setOnContextMenuRequested(std::function<void(int, Point)> callback);
    void setReorderEnabled(bool enabled);
    bool reorderEnabled() const;
    void setOnReorderRequested(std::function<void(int, int)> callback);
    bool setItemDragIds(std::vector<std::wstring> ids);
    void setItemDragEnabled(bool enabled);
    bool itemDragEnabled() const;
    void setOnItemDrag(std::function<void(const ItemDragEvent&)> callback);

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
    void emitItemDrag(ItemDragPhase phase, Point position);
    void updateReorderTarget(Point point);
    void ensureSelectionVisible();
    ListStyle resolvedContainerStyle() const;
    ListStyle resolvedItemStyle(int index) const;
    bool hasInteractionState() const override;
    void resetInteractionState() override;

    std::vector<VirtualListItem> items_;
    SelectionModel selection_;
    int hoveredIndex_ = -1;
    int pressedIndex_ = -1;
    int pressedClickCount_ = 1;
    bool reorderEnabled_ = false;
    bool reordering_ = false;
    bool externalDragging_ = false;
    bool itemDragEnabled_ = false;
    Point reorderStartPoint_{};
    Point reorderCurrentPoint_{};
    int reorderSourceIndex_ = -1;
    int reorderTargetIndex_ = -1;
    int reorderInsertionIndex_ = -1;
    float rowHeight_ = 48.0f;
    VirtualListRichMetrics richMetrics_;
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
    std::function<void(const std::vector<int>&)> onDeleteRequested_;
    std::function<void(int, Point)> onContextMenuRequested_;
    std::function<void(int, int)> onReorderRequested_;
    std::vector<std::wstring> itemDragIds_;
    std::function<void(const ItemDragEvent&)> onItemDrag_;
};

} // namespace oneui

#pragma once

#include "oneui/export.h"
#include "oneui/animation.h"
#include "oneui/input/item_drag.h"
#include "oneui/selection_model.h"
#include "oneui/style.h"
#include "oneui/widget.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace oneui {

struct TableColumn {
    std::wstring header;
    float width = 0.0f;
};

class ONEUI_API Table final : public Widget {
public:
    Table();

    void setColumns(std::vector<TableColumn> columns);
    void setRows(std::vector<std::vector<std::wstring>> rows);
    bool updateRow(std::size_t index, std::vector<std::wstring> row);
    const std::vector<TableColumn>& columns() const;
    const std::vector<std::vector<std::wstring>>& rows() const;
    Rect rowFrame(int index) const;
    void setSelectionMode(SelectionMode mode);
    void setSelectedIndex(int index);
    int selectedIndex() const;
    void setSelectedIndices(std::vector<int> indices);
    const std::vector<int>& selectedIndices() const;
    void setRowHeight(float height);
    float rowHeight() const;
    void setWheelStep(float step);
    void setScrollOffset(float offset);
    float scrollOffset() const;
    float maxScrollOffset() const;
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
    void setStyleOverride(TableStyleOverride style);
    void clearStyleOverride();

    void paint(Canvas& canvas) override;
    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    bool onMouseWheel(const MouseWheelEvent& event) override;
    bool onKeyDown(const KeyEvent& event) override;
    bool isFocusable() const override;
    bool tickAnimations(double nowMs) override;
    AccessibilityInfo accessibilityInfo() const override;

private:
    TableStyle resolvedStyle() const;
    float columnWidth(int index, float remainingWidth, int flexibleCount) const;
    float effectiveRowHeight(float contentHeight) const;
    int hitRowIndex(Point point) const;
    Rect rowRect(int index, float height) const;
    Rect verticalThumbRect(float width, float contentHeight) const;
    void ensureSelectionVisible();
    void notifySelectionChanged(const std::vector<int>& previousIndices, int previousSelectedIndex);
    void resetReorderState();
    void emitItemDrag(ItemDragPhase phase, Point position);
    void updateReorderTarget(Point point);
    bool advanceScrollMotion(double nowMs);
    void resetScrollMotion(float offset);
    bool hasInteractionState() const override;
    void resetInteractionState() override;

    std::vector<TableColumn> columns_;
    std::vector<std::vector<std::wstring>> rows_;
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
    float rowHeight_ = 0.0f;
    float wheelStep_ = 36.0f;
    float scrollOffset_ = 0.0f;
    SmoothScrollMotion scrollMotion_;
    std::optional<TableStyleOverride> styleOverride_;
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

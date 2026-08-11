#pragma once

#include "oneui/export.h"
#include "oneui/style.h"
#include "oneui/widget.h"

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace oneui {

// A structured tree item. `id` must be unique and non-empty. An empty
// parentId denotes a root item; unknown and cyclic parents are shown as roots.
struct TreeItem {
    std::wstring id;
    std::wstring parentId;
    std::wstring title;
    std::wstring detail;
    bool expanded = true;
};

// A small, accessible tree for hierarchical navigation and grouped data.
// It owns selection and expansion locally; callers keep their domain model
// independent and update it through structured items and selected IDs.
class ONEUI_API TreeView final : public Widget {
public:
    TreeView();

    void setItems(std::vector<TreeItem> items);
    void setSelectedId(std::wstring id);
    const std::wstring& selectedId() const;
    bool isExpanded(const std::wstring& id) const;
    void setExpanded(std::wstring id, bool expanded);
    void setStyleOverride(TreeViewStyleOverride style);
    void clearStyleOverride();
    void setOnSelectionChanged(std::function<void(const std::wstring&)> callback);
    void setOnExpansionChanged(std::function<void(const std::wstring&, bool)> callback);
    void setReorderEnabled(bool enabled);
    bool reorderEnabled() const;
    void setOnReorderRequested(
        std::function<void(const std::wstring&, const std::wstring&)> callback);
    void updateExternalDropTarget(Point point);
    void clearExternalDropTarget();
    const std::wstring& externalDropTargetId() const;

    std::size_t visibleItemCount() const;
    float contentHeight() const;

    void paint(Canvas& canvas) override;
    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    bool onKeyDown(const KeyEvent& event) override;
    bool isFocusable() const override;
    AccessibilityInfo accessibilityInfo() const override;

private:
    struct VisibleItem {
        std::size_t index = 0;
        int depth = 0;
    };

    void rebuildHierarchy();
    std::vector<VisibleItem> visibleItems() const;
    void appendVisibleItems(std::vector<VisibleItem>& output, std::size_t index, int depth) const;
    void assignSelectedId(std::wstring id);
    void ensureSelectionVisible();
    int visibleIndexForId(const std::wstring& id, const std::vector<VisibleItem>& visible) const;
    int hitItemIndex(Point point, const std::vector<VisibleItem>& visible) const;
    Rect itemRect(int visibleIndex) const;
    bool isToggleHit(Point point, const VisibleItem& item, int visibleIndex) const;
    void resetReorderState();
    void updateReorderTarget(Point point, const std::vector<VisibleItem>& visible);
    bool hasChildren(std::size_t index) const;
    void toggleItem(std::size_t index);
    std::optional<std::size_t> parentIndex(std::size_t index) const;
    TreeViewStyle resolvedContainerStyle() const;
    TreeViewStyle resolvedItemStyle(bool selected, bool hovered, bool pressed) const;
    float rowHeight() const;
    float toggleWidth() const;
    void updatePreferredHeight();
    bool hasInteractionState() const override;
    void resetInteractionState() override;

    std::vector<TreeItem> items_;
    std::unordered_map<std::wstring, std::size_t> idToIndex_;
    std::vector<std::vector<std::size_t>> children_;
    std::vector<std::size_t> roots_;
    std::unordered_set<std::wstring> expandedIds_;
    std::wstring selectedId_;
    int hoveredIndex_ = -1;
    int pressedIndex_ = -1;
    bool pressedToggle_ = false;
    bool reorderEnabled_ = false;
    bool reordering_ = false;
    Point reorderStartPoint_{};
    int reorderSourceIndex_ = -1;
    int reorderTargetIndex_ = -1;
    int reorderInsertionIndex_ = -1;
    int externalDropTargetIndex_ = -1;
    std::wstring externalDropTargetId_;
    std::optional<TreeViewStyleOverride> styleOverride_;
    std::function<void(const std::wstring&)> onSelectionChanged_;
    std::function<void(const std::wstring&, bool)> onExpansionChanged_;
    std::function<void(const std::wstring&, const std::wstring&)> onReorderRequested_;
};

} // namespace oneui

#pragma once

#include "oneui/export.h"
#include "oneui/style_sheet.h"
#include "oneui/view.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace oneui {

struct ReorderableGridItem {
    std::wstring id;
    std::shared_ptr<Widget> content;
};

/// A responsive native grid for arbitrary child widgets. The control owns
/// layout, drag gesture recognition, hit testing and insertion feedback; the
/// product owns its domain order and applies accepted reorder requests.
class ONEUI_API ReorderableGrid final : public View {
public:
    ReorderableGrid();

    void setItems(std::vector<ReorderableGridItem> items);
    void clearItems();
    void addItem(ReorderableGridItem item);
    bool moveItem(const std::wstring& sourceId, int targetIndex);
    int itemCount() const;

    void setColumnCount(int columns);
    int columnCount() const;
    void setColumnGap(float gap);
    void setRowGap(float gap);
    void setItemHeight(float height);
    float contentHeight() const;

    void setStyleBox(StyleBox style);
    void setReorderEnabled(bool enabled);
    bool reorderEnabled() const;
    void setOnReorderRequested(std::function<void(const std::wstring&, int)> callback);

    void paint(Canvas& canvas) override;
    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;

protected:
    void layoutChildren() override;
    void resetInteractionState() override;

private:
    int itemIndexAt(Point point) const;
    int insertionIndexAt(Point point) const;
    void updateReorderTarget(Point point);
    void resetReorderState();
    void updatePreferredHeight();
    Insets padding() const;
    float resolvedColumnGap() const;
    float resolvedRowGap() const;
    float resolvedItemHeight() const;
    Color indicatorColor() const;
    float indicatorWidth() const;

    std::vector<ReorderableGridItem> items_;
    std::unordered_set<std::wstring> itemIds_;
    int columns_ = 1;
    float columnGap_ = 0.0f;
    float rowGap_ = 0.0f;
    float itemHeight_ = 160.0f;
    StyleBox style_;
    bool reorderEnabled_ = false;
    bool reordering_ = false;
    Point reorderStartPoint_;
    int reorderSourceIndex_ = -1;
    int reorderTargetIndex_ = -1;
    int reorderInsertionIndex_ = -1;
    std::function<void(const std::wstring&, int)> onReorderRequested_;
};

} // namespace oneui

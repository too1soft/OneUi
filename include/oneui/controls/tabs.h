#pragma once

#include "oneui/export.h"
#include "oneui/icon.h"
#include "oneui/reactive.h"
#include "oneui/style.h"
#include "oneui/widget.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace oneui {

enum class TabsSizingMode {
    Equal,
    Compact
};

class ONEUI_API Tabs final : public Widget {
public:
    Tabs();

    void setItems(std::vector<std::wstring> items);
    const std::vector<std::wstring>& items() const;
    Rect itemFrame(int index) const;
    Rect itemTextFrame(int index) const;
    float itemPaintFontSize(int index) const;
    int itemPaintFontWeight(int index) const;
    TextAlign itemTextAlign(int index) const;
    void setItemIcons(std::vector<std::optional<IconSymbol>> icons);
    void setSelectedIndex(int index);
    int selectedIndex() const;
    void bindSelectedIndex(State<int>& state);
    void setStyleOverride(TabsStyleOverride style);
    void clearStyleOverride();
    void setSizingMode(TabsSizingMode mode);
    TabsSizingMode sizingMode() const;
    void setItemWidthRange(float minimum, float maximum);
    void setClosable(bool closable);
    bool closable() const;
    void setReorderEnabled(bool enabled);
    bool reorderEnabled() const;
    void setOnChanged(std::function<void(int)> callback);
    void setOnCloseRequested(std::function<void(int)> callback);
    void setOnContextMenuRequested(std::function<void(int, Point)> callback);
    void setOnReorderRequested(std::function<void(int, int)> callback);

    void paint(Canvas& canvas) override;
    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    bool onMouseWheel(const MouseWheelEvent& event) override;
    bool onKeyDown(const KeyEvent& event) override;
    bool isFocusable() const override;
    AccessibilityInfo accessibilityInfo() const override;

private:
    void updateCompactMetrics(Canvas& canvas);
    float itemWidth(int index) const;
    float itemOffset(int index) const;
    float contentWidth() const;
    float maximumScrollOffset() const;
    Rect itemRect(int index) const;
    Rect textRect(int index) const;
    Rect closeRect(int index) const;
    int hitIndex(Point point) const;
    int hitCloseIndex(Point point) const;
    void ensureIndexVisible(int index);
    TabsStyle resolvedContainerStyle() const;
    TabsStyle resolvedItemStyle(int index) const;
    void assignSelectedIndex(int index);
    bool hasInteractionState() const override;
    void resetInteractionState() override;

    std::vector<std::wstring> items_;
    std::vector<std::optional<IconSymbol>> itemIcons_;
    int selectedIndex_ = 0;
    int hoveredIndex_ = -1;
    int pressedIndex_ = -1;
    int hoveredCloseIndex_ = -1;
    int pressedCloseIndex_ = -1;
    int contextPressedIndex_ = -1;
    int dragSourceIndex_ = -1;
    int dragTargetIndex_ = -1;
    Point dragStart_{};
    bool dragging_ = false;
    TabsSizingMode sizingMode_ = TabsSizingMode::Equal;
    float minimumItemWidth_ = 96.0f;
    float maximumItemWidth_ = 220.0f;
    std::vector<float> compactItemOffsets_;
    float scrollOffset_ = 0.0f;
    bool closable_ = false;
    bool reorderEnabled_ = false;
    Binding<int> selectedBinding_;
    std::optional<TabsStyleOverride> styleOverride_;
    std::function<void(int)> onChanged_;
    std::function<void(int)> onCloseRequested_;
    std::function<void(int, Point)> onContextMenuRequested_;
    std::function<void(int, int)> onReorderRequested_;
};

} // namespace oneui

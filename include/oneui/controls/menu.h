#pragma once

#include "oneui/export.h"
#include "oneui/icon.h"
#include "oneui/style_sheet.h"
#include "oneui/widget.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace oneui {

// Menu 是标准下拉/上下文菜单面：可选状态头 + 条目（图标 + 文本，支持 danger
// 变体与禁用）+ 分隔线，自绘并跟踪 hover/pressed。样式由 StyleSheet 驱动：
//   menu            面板底面（background/border/border-radius/padding/box-shadow）
//   .menu-item      条目（:hover/:active/:disabled；.danger 类为危险动作）
//   .menu-separator 分隔线（background）
// 条目激活通过 setOnItemActivated 回调条目序号（仅 Item 计数）。
class ONEUI_API Menu final : public Widget {
public:
    Menu();

    void addHeader(std::wstring title, std::wstring subtitle);
    int addItem(std::wstring text, std::optional<IconSymbol> icon = std::nullopt, bool danger = false);
    void addSeparator();
    void setItemDisabled(int index, bool disabled);
    void setOnItemActivated(std::function<void(int)> callback);
    void setStyleSheet(std::shared_ptr<StyleSheet> sheet, StyleNode node);

    // preferredHeight 按当前条目返回自然高度（供调用方设置 overlay 尺寸）。
    float preferredHeight() const;

    void paint(Canvas& canvas) override;
    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    bool isFocusable() const override;

protected:
    bool hasInteractionState() const override;
    void resetInteractionState() override;

private:
    struct Entry {
        enum class Kind { Header, Item, Separator };
        Kind kind = Kind::Item;
        std::wstring title;
        std::wstring subtitle;
        std::optional<IconSymbol> icon;
        bool danger = false;
        bool disabled = false;
        int itemIndex = -1;
    };

    float entryHeight(const Entry& entry) const;
    Insets surfacePadding() const;
    StyleBox resolvedSurface() const;
    StyleBox resolvedItemStyle(const Entry& entry, bool hovered, bool pressed) const;
    int entryAt(Point point) const; // 命中的可交互条目在 entries_ 中的下标，无则 -1
    Rect entryRect(int entryIndex) const;

    std::vector<Entry> entries_;
    int itemCount_ = 0;
    int hoveredEntry_ = -1;
    int pressedEntry_ = -1;
    std::function<void(int)> onItemActivated_;
    std::shared_ptr<StyleSheet> styleSheet_;
    StyleNode styleNode_{"menu", {}, StyleStateNone};
};

} // namespace oneui

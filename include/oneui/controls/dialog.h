#pragma once

#include "oneui/export.h"
#include "oneui/icon.h"
#include "oneui/style_sheet.h"
#include "oneui/view.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace oneui {

// Dialog 是标准模态弹窗骨架：头部（可选图标徽章 + 标题/副标题 + 关闭键）+
// 内容槽 + 底部动作槽。头部自绘、槽位走子组件布局。样式由 StyleSheet 驱动：
//   dialog        弹窗面（background/border/border-radius/padding/box-shadow/
//                 color/font-size/font-weight 作用于标题）
//   icon-badge    头部图标徽章（与独立 IconBadge 组件同一规则，全局一致）
//   .dialog-close 关闭键（:hover/:active）
// 遮罩（scrim）与居中定位属 overlay 编排，由调用方通过 OverlayHost 完成。
class ONEUI_API Dialog final : public View {
public:
    Dialog(std::wstring title = {}, std::wstring subtitle = {});

    void setTitle(std::wstring title);
    void setSubtitle(std::wstring subtitle);
    void setIconSymbol(IconSymbol symbol);
    void clearIconSymbol();
    void setCloseVisible(bool visible);
    void setOnClose(std::function<void()> callback);
    void setContent(std::shared_ptr<Widget> content);
    void setActions(std::shared_ptr<Widget> actions);
    void setStyleSheet(std::shared_ptr<StyleSheet> sheet, StyleNode node);

    void paint(Canvas& canvas) override;
    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    CursorKind cursor(Point point) const override;

protected:
    void layoutChildren() override;
    bool hasInteractionState() const override;
    void resetInteractionState() override;

private:
    struct HeaderLayout {
        Rect icon;
        Rect title;
        Rect subtitle;
        Rect close;
        float height = 0.0f;
    };

    StyleBox resolvedSurface() const;
    HeaderLayout headerLayout() const;
    Rect contentArea() const;
    void rebuildChildren();

    std::wstring title_;
    std::wstring subtitle_;
    std::optional<IconSymbol> icon_;
    bool closeVisible_ = true;
    bool closeHovered_ = false;
    bool closePressed_ = false;
    std::function<void()> onClose_;
    std::shared_ptr<Widget> content_;
    std::shared_ptr<Widget> actions_;
    std::shared_ptr<StyleSheet> styleSheet_;
    StyleNode styleNode_{"dialog", {}, StyleStateNone};
};

} // namespace oneui

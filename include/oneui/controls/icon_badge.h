#pragma once

#include "oneui/export.h"
#include "oneui/icon.h"
#include "oneui/style_sheet.h"
#include "oneui/widget.h"

namespace oneui {

// IconBadge 在圆角底面上居中绘制一枚矢量图标，用于卡片头像、弹窗标识、
// 空状态插图等场景。外观由 StyleSheet 驱动（tag: icon-badge）：
// background/border/border-radius/padding 决定底面，color 决定图标主色；
// accent 是图标的点缀色，属内容而非样式，走属性设置。
class ONEUI_API IconBadge final : public Widget {
public:
    explicit IconBadge(IconSymbol symbol = IconSymbol::BrandBloom);

    void setSymbol(IconSymbol symbol);
    IconSymbol symbol() const;
    void setAccent(Color accent);
    void setStrokeWidth(float width);
    void setStyleBox(StyleBox box);

    void paint(Canvas& canvas) override;

private:
    IconSymbol symbol_;
    Color accent_{0, 0, 0, 0};
    float strokeWidth_ = 1.6f;
    StyleBox styleBox_{};
};

} // namespace oneui

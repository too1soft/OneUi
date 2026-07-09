#pragma once

#include "oneui/canvas.h"
#include "oneui/export.h"
#include "oneui/geometry.h"
#include "oneui/style_sheet.h"
#include "oneui/view.h"

#include <optional>

namespace oneui {

class ONEUI_API Panel final : public View {
public:
    void setBackground(Color color);
    void setBorder(Color color);
    void setBorderWidth(float width);
    void setRadius(float radius);
    void setPadding(Insets padding);
    void setShadow(BoxShadow shadow);
    void setStyleBox(StyleBox style);
    void clearStyleBox();
    void setContent(std::shared_ptr<Widget> child);

    void paint(Canvas& canvas) override;

private:
    void layoutChildren() override;

    Color background_{255, 255, 255, 255};
    Color border_{226, 229, 234, 255};
    float borderWidth_ = 1.0f;
    // 默认无圆角，与 CSS 对齐（div 默认 border-radius:0）；需要圆角时显式
    // setRadius 或走 StyleSheet 的 border-radius。默认 8 曾让纯色内容区面板
    // 与标题栏交界处出现意外圆角割裂。
    float radius_ = 0.0f;
    Insets padding_;
    BoxShadow shadow_{Color{0, 0, 0, 0}, Point{0.0f, 0.0f}, 0.0f, 0.0f};
    std::optional<StyleBox> styleBox_;
};

} // namespace oneui

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
    float radius_ = 8.0f;
    Insets padding_;
    BoxShadow shadow_{Color{0, 0, 0, 0}, Point{0.0f, 0.0f}, 0.0f, 0.0f};
    std::optional<StyleBox> styleBox_;
};

} // namespace oneui

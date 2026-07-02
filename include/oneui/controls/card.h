#pragma once

#include "oneui/export.h"
#include "oneui/style_sheet.h"
#include "oneui/view.h"

#include <optional>

namespace oneui {

class ONEUI_API Card : public View {
public:
    void setBackground(Color color);
    void setBorder(Color color);
    void setRadius(float radius);
    void setPadding(Insets padding);
    void setShadow(BoxShadow shadow);
    void setStyleBox(StyleBox style);
    void clearStyleBox();
    void setContent(std::shared_ptr<Widget> child);

    void paint(Canvas& canvas) override;

private:
    void layoutChildren() override;

    Color background_{255, 255, 255};
    Color border_{226, 229, 234};
    float radius_ = 8.0f;
    Insets padding_{0.0f};
    BoxShadow shadow_{Color{15, 23, 42, 38}, Point{0.0f, 2.0f}, 10.0f, 0.0f};
    std::optional<StyleBox> styleBox_;
};

} // namespace oneui

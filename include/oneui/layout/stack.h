#pragma once

#include "oneui/export.h"
#include "oneui/geometry.h"
#include "oneui/style_sheet.h"
#include "oneui/view.h"

#include <optional>

namespace oneui {

enum class StackDirection {
    Row,
    Column
};

enum class StackAlign {
    Start,
    Center,
    End,
    Stretch
};

class ONEUI_API Stack final : public View {
public:
    explicit Stack(StackDirection direction = StackDirection::Column);

    void setDirection(StackDirection direction);
    void setGap(float gap);
    void setPadding(Insets padding);
    void setAlign(StackAlign align);
    void setStyleBox(StyleBox style);
    void clearStyleBox();
    float contentWidth() const;
    float contentHeight() const;

    void paint(Canvas& canvas) override;

private:
    void layoutChildren() override;

    StackDirection direction_ = StackDirection::Column;
    float gap_ = 0.0f;
    Insets padding_;
    StackAlign align_ = StackAlign::Stretch;
    std::optional<StyleBox> styleBox_;
};

} // namespace oneui

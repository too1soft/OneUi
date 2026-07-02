#pragma once

#include "oneui/export.h"
#include "oneui/geometry.h"
#include "oneui/view.h"

namespace oneui {

class ONEUI_API Wrap final : public View {
public:
    void setGap(float gap);
    void setRowGap(float gap);
    void setPadding(Insets padding);

private:
    void layoutChildren() override;

    float gap_ = 0.0f;
    float rowGap_ = 0.0f;
    Insets padding_;
};

} // namespace oneui

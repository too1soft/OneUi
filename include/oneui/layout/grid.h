#pragma once

#include "oneui/export.h"
#include "oneui/geometry.h"
#include "oneui/view.h"

namespace oneui {

class ONEUI_API Grid final : public View {
public:
    explicit Grid(int columns = 1);

    void setColumns(int columns);
    void setGap(float gap);
    void setColumnGap(float gap);
    void setRowGap(float gap);
    void setPadding(Insets padding);
    void setAutoRows(float height);

private:
    void layoutChildren() override;

    int columns_ = 1;
    float columnGap_ = 0.0f;
    float rowGap_ = 0.0f;
    float autoRows_ = 0.0f;
    Insets padding_;
};

} // namespace oneui

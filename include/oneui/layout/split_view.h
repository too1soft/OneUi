#pragma once

#include "oneui/export.h"
#include "oneui/geometry.h"
#include "oneui/view.h"

#include <memory>

namespace oneui {

enum class SplitOrientation {
    Horizontal,
    Vertical
};

class ONEUI_API SplitView final : public View {
public:
    explicit SplitView(SplitOrientation orientation = SplitOrientation::Horizontal);

    void setFirst(std::shared_ptr<Widget> child);
    void setSecond(std::shared_ptr<Widget> child);
    void setOrientation(SplitOrientation orientation);
    void setSplitRatio(float ratio);
    void setGap(float gap);
    void setPadding(Insets padding);

private:
    void layoutChildren() override;
    void rebuildChildren();

    std::shared_ptr<Widget> first_;
    std::shared_ptr<Widget> second_;
    SplitOrientation orientation_ = SplitOrientation::Horizontal;
    float splitRatio_ = 0.5f;
    float gap_ = 0.0f;
    Insets padding_;
};

} // namespace oneui

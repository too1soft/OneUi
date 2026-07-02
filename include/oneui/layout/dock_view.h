#pragma once

#include "oneui/export.h"
#include "oneui/geometry.h"
#include "oneui/view.h"

#include <memory>

namespace oneui {

class ONEUI_API DockView final : public View {
public:
    void setTop(std::shared_ptr<Widget> child);
    void setRight(std::shared_ptr<Widget> child);
    void setBottom(std::shared_ptr<Widget> child);
    void setLeft(std::shared_ptr<Widget> child);
    void setCenter(std::shared_ptr<Widget> child);
    void setGap(float gap);
    void setPadding(Insets padding);

private:
    void layoutChildren() override;
    void rebuildChildren();

    std::shared_ptr<Widget> top_;
    std::shared_ptr<Widget> right_;
    std::shared_ptr<Widget> bottom_;
    std::shared_ptr<Widget> left_;
    std::shared_ptr<Widget> center_;
    float gap_ = 0.0f;
    Insets padding_;
};

} // namespace oneui

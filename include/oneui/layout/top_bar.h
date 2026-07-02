#pragma once

#include "oneui/geometry.h"
#include "oneui/style_sheet.h"
#include "oneui/view.h"

#include <memory>
#include <optional>
#include <vector>

namespace oneui {

class ONEUI_API TopBar final : public View {
public:
    void setLeading(std::shared_ptr<Widget> child);
    void addAction(std::shared_ptr<Widget> child);
    void clearActions();

    void setPadding(Insets padding);
    void setGap(float gap);
    void setLeadingWidth(float width);
    void setStyleBox(StyleBox style);
    void clearStyleBox();

    float gap() const;
    float leadingWidth() const;

    void paint(Canvas& canvas) override;

private:
    void layoutChildren() override;
    void rebuildChildren();

    std::shared_ptr<Widget> leading_;
    std::vector<std::shared_ptr<Widget>> actions_;
    Insets padding_;
    float gap_ = 10.0f;
    float leadingWidth_ = 0.0f;
    std::optional<StyleBox> styleBox_;
};

} // namespace oneui

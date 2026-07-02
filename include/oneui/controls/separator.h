#pragma once

#include "oneui/export.h"
#include "oneui/style.h"
#include "oneui/widget.h"

#include <optional>

namespace oneui {

enum class SeparatorOrientation {
    Horizontal,
    Vertical
};

class ONEUI_API Separator final : public Widget {
public:
    explicit Separator(SeparatorOrientation orientation = SeparatorOrientation::Horizontal);

    void setOrientation(SeparatorOrientation orientation);
    SeparatorOrientation orientation() const;
    void setStyleOverride(SeparatorStyleOverride style);
    void clearStyleOverride();

    void paint(Canvas& canvas) override;

private:
    SeparatorStyle resolvedStyle() const;

    SeparatorOrientation orientation_ = SeparatorOrientation::Horizontal;
    std::optional<SeparatorStyleOverride> styleOverride_;
};

} // namespace oneui

#pragma once

#include "oneui/animation.h"
#include "oneui/export.h"
#include "oneui/style_sheet.h"

namespace oneui {

class ONEUI_API StyleBoxTransition final {
public:
    void reset(const StyleBox& box);
    void animateTo(const StyleBox& from, const StyleBox& target, double nowMs);
    bool tick(double nowMs);
    bool initialized() const;
    bool running() const;
    StyleBox applyTo(StyleBox target) const;

private:
    ColorTransition background_{Color{0, 0, 0, 0}};
    ColorTransition foreground_{Color{0, 0, 0, 0}};
    ColorTransition border_{Color{0, 0, 0, 0}};
    FloatTransition opacity_{1.0f};
    bool initialized_ = false;
};

} // namespace oneui

#pragma once

#include "oneui/canvas.h"
#include <cstdlib>

namespace oneui::text {
class Layout;
}

namespace oneui::rendering {
class PreparedTextCanvas {
public:
    virtual ~PreparedTextCanvas() = default;
    virtual bool paintPreparedText(const text::Layout& layout, Point origin, Color color,
                                   bool useCanvasDefaultFamily) = 0;
};

inline bool tryPaintPreparedText(Canvas& canvas, const text::Layout& layout, Point origin, Color color,
                                 bool useCanvasDefaultFamily) {
    static const bool enabled = [] {
        const char* value = std::getenv("ONEUI_EXPERIMENT_PREPARED_LABEL_LAYOUT");
        return value && value[0] == '1' && value[1] == '\0';
    }();
    if (!enabled) return false;
    auto* prepared = dynamic_cast<PreparedTextCanvas*>(&canvas);
    return prepared && prepared->paintPreparedText(layout, origin, color, useCanvasDefaultFamily);
}
}

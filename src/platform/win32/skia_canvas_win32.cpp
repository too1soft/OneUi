#include "skia_canvas_win32.h"

#include "include/core/SkMilestone.h"
#include "include/core/SkPath.h"
#if SK_MILESTONE >= 150
#include "include/core/SkPathBuilder.h"
#endif

namespace oneui::win32 {

SkPath toSkPath(const CanvasPath& path) {
#if SK_MILESTONE >= 150
    SkPathBuilder builder;
#else
    SkPath native;
#endif
    for (const auto& command : path.commands) {
        switch (command.verb) {
        case CanvasPathVerb::MoveTo:
#if SK_MILESTONE >= 150
            builder.moveTo(command.first.x, command.first.y);
#else
            native.moveTo(command.first.x, command.first.y);
#endif
            break;
        case CanvasPathVerb::LineTo:
#if SK_MILESTONE >= 150
            builder.lineTo(command.first.x, command.first.y);
#else
            native.lineTo(command.first.x, command.first.y);
#endif
            break;
        case CanvasPathVerb::CubicTo:
#if SK_MILESTONE >= 150
            builder.cubicTo(
#else
            native.cubicTo(
#endif
                command.first.x,
                command.first.y,
                command.second.x,
                command.second.y,
                command.third.x,
                command.third.y);
            break;
        case CanvasPathVerb::Close:
#if SK_MILESTONE >= 150
            builder.close();
#else
            native.close();
#endif
            break;
        }
    }
#if SK_MILESTONE >= 150
    return builder.detach();
#else
    return native;
#endif
}

} // namespace oneui::win32

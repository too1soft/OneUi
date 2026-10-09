#include "skia_path.h"

#include "include/core/SkMilestone.h"
#include "include/core/SkPath.h"
#include <array>
#include <utility>
#if SK_MILESTONE >= 150
#include "include/core/SkPathBuilder.h"
#endif

namespace oneui::rendering {

namespace {
SkPath reuseSmallPath(SkPath path) {
    // Stable native identities let Ganesh reuse the rasterized masks of static
    // icons/rings. CanvasPath's public command vector can change directly, so
    // compare geometry, never the caller's address. Large animated charts bypass
    // this cache; the per-thread cache holds at most 16 paths of 512 points.
    if (path.isEmpty() || !path.isFinite() || path.countPoints() > 512 || path.countVerbs() > 512)
        return path;
    static thread_local std::array<SkPath, 16> cache;
    for (std::size_t i = 0; i < cache.size(); ++i) {
        if (cache[i] == path) {
            SkPath hit = cache[i];
            for (; i > 0; --i) cache[i] = std::move(cache[i-1]);
            cache[0] = hit;
            return hit;
        }
    }
    for (std::size_t i = cache.size()-1; i > 0; --i) cache[i] = std::move(cache[i-1]);
    cache[0] = path;
    return path;
}
} // namespace

SkPath toSkPath(const CanvasPath &path) {
#if SK_MILESTONE >= 150
    SkPathBuilder builder;
#else
    SkPath native;
#endif
    for (const auto &command : path.commands) {
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
                command.first.x, command.first.y, command.second.x, command.second.y, command.third.x,
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
    return reuseSmallPath(builder.detach());
#else
    return reuseSmallPath(std::move(native));
#endif
}

} // namespace oneui::rendering

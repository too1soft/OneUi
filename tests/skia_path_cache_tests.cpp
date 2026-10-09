#include "platform/shared/skia_path.h"
#include "include/core/SkPath.h"
#include <iostream>

int main() {
    int failures = 0;
    const auto check = [&](bool value, const char* message) {
        if (!value) { std::cerr << message << '\n'; ++failures; }
    };
    oneui::CanvasPath source;
    source.moveTo({1, 2}); source.lineTo({20, 30});
    const auto first = oneui::rendering::toSkPath(source);
    const auto originalId = first.getGenerationID();
    auto copy = source;
    check(oneui::rendering::toSkPath(copy).getGenerationID() == originalId,
          "equal geometry in another CanvasPath lost native identity");
    copy.commands[1].first.x = 80;
    const auto changed = oneui::rendering::toSkPath(copy);
    check(changed.getGenerationID() != originalId && changed.getBounds().right() == 80,
          "direct command mutation reused stale geometry");
    check(first.getBounds().right() == 20, "cache mutation changed an outstanding path");
    // Eviction must not corrupt paths still held by callers or queued GPU work.
    for (int i = 0; i < 40; ++i) {
        copy.commands[1].first.x = 100.f+i;
        oneui::rendering::toSkPath(copy);
    }
    check(first.getBounds().right() == 20, "eviction changed an outstanding path");
    check(oneui::rendering::toSkPath(source) == first, "evicted path changed geometry");
    // Large dynamic geometry must not evict a hot small path.
    const auto hot = oneui::rendering::toSkPath(source);
    for (int j = 0; j < 20; ++j) {
        oneui::CanvasPath large;
        large.moveTo({0,0});
        for (int i = 0; i < 600; ++i) large.lineTo({float(i),float(j+i%7)});
        oneui::rendering::toSkPath(large);
    }
    check(oneui::rendering::toSkPath(source).getGenerationID() == hot.getGenerationID(),
          "large paths polluted the small-path cache");
    return failures ? 1 : 0;
}

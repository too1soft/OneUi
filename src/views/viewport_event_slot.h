#pragma once
#include "event_dispatch.h"

namespace oneui::views {
inline constexpr uint32_t viewportSizeEventId = 1;

inline bool hasViewportSizeEvent(const Instance& instance) {
    return instance.element.component == "Scroll" &&
        std::any_of(instance.events.begin(), instance.events.end(),
                    [](const auto& record) { return record[1] == viewportSizeEventId; });
}

inline std::function<void(Size)> viewportSizeCallback(const std::shared_ptr<Instance>& instance) {
    return [fire = eventDispatcher(instance, viewportSizeEventId)](Size size) {
        Value width, height;
        width.tag = height.tag = 3;
        width.number = size.width;
        height.number = size.height;
        fire({width, height});
    };
}
} // namespace oneui::views

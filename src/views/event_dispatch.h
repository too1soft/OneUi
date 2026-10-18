#pragma once
#include "runtime.h"
#include <algorithm>

namespace oneui::views {
inline std::function<void(std::vector<Value>)> eventDispatcher(
    std::weak_ptr<Instance> node, uint32_t id) {
    return [node, id](std::vector<Value> args) {
        const auto item = node.lock();
        const auto runtime = item ? item->owner.lock() : nullptr;
        if (!runtime || runtime->closing || item->applying) return;
        const auto found = std::find_if(item->events.begin(), item->events.end(),
            [&](const auto& event) { return event[1] == id; });
        if (found == item->events.end()) return;
        const auto event = *found;
        try { runtime->invoke(item, event, std::move(args)); }
        catch (const Failure& error) { runtime->fail(error.result, error.what(), event[7]); }
        catch (...) {
            runtime->fail(ONEUI_VIEW_FOREIGN_FAILURE, "Exception in command callback", event[7]);
        }
    };
}
} // namespace oneui::views

#pragma once
#include "oneui/export.h"
#include <cstdint>
#include <functional>

namespace oneui {
// Native content reports measured cells; the workspace never guesses font
// metrics. This C++ engine interface does not add a C ABI entry point.
struct ContentViewport { std::uint16_t rows = 1, columns = 1; };
class ONEUI_API ContentViewportSource {
public:
    virtual ~ContentViewportSource() = default;
    virtual void holdViewportNotifications(bool) = 0;
    virtual void setWorkspaceViewportObserver(std::function<void(ContentViewport)>) = 0;
};
} // namespace oneui

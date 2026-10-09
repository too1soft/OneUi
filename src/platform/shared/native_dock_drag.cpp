#include "native_dock_drag.h"
#if defined(__linux__)
#include "platform/linux/linux_runtime.h"
#endif
namespace oneui::platform {
#ifdef _WIN32
std::shared_ptr<NativeDockDrag> makeWinDockDrag(Window &, DockDragCallbacks);
#elif defined(__APPLE__)
std::shared_ptr<NativeDockDrag> makeCocoaDockDrag(Window &, DockDragCallbacks);
#endif
std::shared_ptr<NativeDockDrag> makeNativeDockDrag(Window &window, DockDragCallbacks callbacks) {
#ifdef _WIN32
    return makeWinDockDrag(window, std::move(callbacks));
#elif defined(__APPLE__)
    return makeCocoaDockDrag(window, std::move(callbacks));
#elif defined(__linux__)
    return linux_platform::connection()->dockDrag(window, std::move(callbacks));
#else
    (void)window;
    (void)callbacks;
    return {};
#endif
}
} // namespace oneui::platform

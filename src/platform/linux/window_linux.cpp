#include "linux_runtime.h"
#include <cstdlib>
#include <iostream>

#include <stdexcept>
#include <utility>

namespace oneui::linux_platform {
std::shared_ptr<Connection> connection() {
    static std::shared_ptr<Connection> instance = [] {
        const char* value = std::getenv("ONEUI_LINUX_BACKEND");
        const std::string requested = value ? value : "auto";
        if (requested != "auto" && requested != "x11" && requested != "wayland")
            throw std::runtime_error("ONEUI_LINUX_BACKEND must be auto, x11 or wayland");
        if (requested == "wayland") return waylandConnection();
        if (requested == "x11") return x11Connection();
        if (std::getenv("WAYLAND_DISPLAY")) {
            try { return waylandConnection(); }
            catch (const std::exception& e) {
                if (!std::getenv("DISPLAY")) throw;
                std::cerr << "OneUI: Wayland initialization failed; trying X11: " << e.what() << '\n';
            }
        }
        return x11Connection();
    }();
    instance->assertUiThread();
    return instance;
}
}
namespace oneui {

std::unique_ptr<Window> Window::create(std::wstring title, int width, int height) {
    WindowOptions options;
    options.title = std::move(title);
    options.width = width;
    options.height = height;
    return Window::create(std::move(options));
}

std::unique_ptr<Window> Window::create(WindowOptions options) {
    return linux_platform::connection()->create(std::move(options));
}

void SystemClipboard::setText(std::wstring text) {
    linux_platform::connection()->setClipboard(text);
}

std::wstring SystemClipboard::text() const {
    return linux_platform::connection()->clipboard();
}

std::vector<MonitorInfo> enumerateMonitors() { return linux_platform::connection()->monitors(); }

} // namespace oneui

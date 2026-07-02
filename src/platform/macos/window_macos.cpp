#include "oneui/platform/window.h"

#include <stdexcept>
#include <utility>

namespace oneui {

// macOS skeleton entry point. Keep this backend unwired until the Cocoa window,
// event, input, DPI, font, clipboard, and Skia presentation paths exist.
std::unique_ptr<Window> Window::create(std::wstring title, int width, int height) {
    WindowOptions options;
    options.title = std::move(title);
    options.width = width;
    options.height = height;
    return Window::create(std::move(options));
}

std::unique_ptr<Window> Window::create(WindowOptions options) {
    (void)options;
    throw std::logic_error("OneUI macOS window backend is not implemented yet; Win32 is the only supported backend.");
}

void SystemClipboard::setText(std::wstring text) {
    (void)text;
    throw std::logic_error("OneUI macOS clipboard backend is not implemented yet.");
}

std::wstring SystemClipboard::text() const {
    throw std::logic_error("OneUI macOS clipboard backend is not implemented yet.");
}

} // namespace oneui

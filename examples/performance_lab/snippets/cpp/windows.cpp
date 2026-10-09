#include <oneui/platform/window.h>

oneui::Window::EventSubscription example_window_state(oneui::Window &window) {
    window.setMinimumClientSize({640, 480});
    window.setMaximumClientSize({1600, 1200});
    window.setResizable(true);
    // Retain the returned subscription. Requests may be refused by the OS.
    return window.observeState([](const oneui::WindowState &actual) {
        if (actual.knownFields & std::uint32_t(oneui::WindowStateField::Maximized)) {
            const bool maximized = actual.maximized;
            (void)maximized;
        }
    });
}

#pragma once
#include "oneui/ui.h"
#include "oneui/platform/window.h"

namespace oneui::ui {
// Window defaults and lifetime are owned here; advanced apps can access window().
class App {
public:
    explicit App(std::wstring title, int width = 1100, int height = 760) {
        if (width <= 0 || height <= 0) throw std::invalid_argument("Invalid window size");
        WindowOptions options; options.title = std::move(title); options.width = width; options.height = height;
        window_ = Window::create(options); window_->setDefaultFontFamily(L"Microsoft YaHei UI");
    }
    Window& window() const { return *window_; }
    int run(Node root, std::function<void()> ready = {}) {
        window_->setContent(root.widget); window_->initialize(); window_->centerOnActiveMonitor();
        window_->show(); window_->activate();
        if (ready) ready();
        return window_->run();
    }
private:
    std::unique_ptr<Window> window_;
};
} // namespace oneui::ui

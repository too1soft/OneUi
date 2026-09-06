#include "oneui/oneui.h"
#include <memory>
int main() {
    auto window = oneui::Window::create(L"OneUI C++ SDK consumer", 400, 240);
    auto field = std::make_shared<oneui::TextField>(L"SDK / 中文 / emoji");
    window->setContent(field);
    window->show();
    if (window->backend() == oneui::WindowBackend::Unknown) return 1;
    window->requestAnimationFrame([&](double) { window->close(); });
    return window->run();
}

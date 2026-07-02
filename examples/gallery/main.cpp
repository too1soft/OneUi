#include "gallery_view.h"

#include "oneui/platform/window.h"

#include <memory>

int main() {
    auto window = oneui::Window::create(L"OneUI Gallery", 1040, 580);
    window->setContent(std::make_shared<oneui::gallery::GalleryView>());
    window->show();
    return window->run();
}

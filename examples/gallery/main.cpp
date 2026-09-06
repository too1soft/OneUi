#include "gallery_view.h"

#include "oneui/platform/window.h"

#include <memory>
#include <string_view>

int main(int argc, char** argv) {
    auto window = oneui::Window::create(L"OneUI Gallery", 1040, 580);
    window->setContent(std::make_shared<oneui::gallery::GalleryView>());
    window->show();
    const bool smoke = argc > 1 && std::string_view(argv[1]) == "--smoke-test";
    const bool capture = argc > 1 && std::string_view(argv[1]) == "--capture-frame";
    bool captured = !capture;
    int frames = 0;
    std::function<void(double)> advance;
    if (smoke || capture) {
        advance = [&](double) {
            if (++frames < 4) window->requestAnimationFrame(advance);
            else {
                if (capture) captured = window->captureFramePng(L"oneui-gallery.png");
                window->close();
            }
        };
        window->requestAnimationFrame(advance);
    }
    const int result = window->run();
    return captured ? result : 2;
}

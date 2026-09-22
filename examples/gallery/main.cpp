#include "gallery_view.h"

#include "oneui/platform/window.h"

#include <memory>
#include <cstdio>
#include <string_view>

int main(int argc, char** argv) {
    const bool smoke = argc > 1 && std::string_view(argv[1]) == "--smoke-test";
    const bool capture = argc > 1 && std::string_view(argv[1]) == "--capture-frame";
    if (smoke || capture) std::fprintf(stderr, "Gallery acceptance: creating window\n");
    auto window = oneui::Window::create(L"OneUI Gallery", 1040, 580);
    window->setContent(std::make_shared<oneui::gallery::GalleryView>());
    if (smoke || capture) std::fprintf(stderr, "Gallery acceptance: showing window\n");
    window->show();
    if (smoke || capture) std::fprintf(stderr, "Gallery acceptance: window shown\n");
    bool captured = !capture;
    int frames = 0;
    std::function<void(double)> advance;
    if (smoke || capture) {
        advance = [&](double) {
            std::fprintf(stderr, "Gallery acceptance: frame %d\n", frames + 1);
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

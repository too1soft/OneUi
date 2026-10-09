#include "drawing_preview.hpp"
#include "pages/page_factories.hpp"
#include "support/recording_canvas.h"
#include <chrono>
#include <iostream>
#include <oneui/controls/image_view.h>
#include <stdexcept>
#include <thread>

using namespace oneui;
namespace {
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class T>
std::shared_ptr<T> find(const std::shared_ptr<Widget> &root, const wchar_t *name = L"") {
    if (auto item = std::dynamic_pointer_cast<T>(root);
        item && (!*name || item->accessibleName() == name))
        return item;
    if (auto view = std::dynamic_pointer_cast<View>(root))
        for (const auto &child : view->children())
            if (auto item = find<T>(child, name))
                return item;
    return {};
}
void key(Widget &widget, Key value) {
    KeyEvent event;
    event.key = value;
    require(widget.onKeyDown(event), "Media control did not handle key");
}
void next(Select &select) {
    key(select, Key::Enter);
    key(select, Key::Down);
    key(select, Key::Enter);
}
void drawingParameters() {
    lab::DrawingPreview sample;
    sample.setFrame({15, 20, 320, 230});
    sample.setOpacity(.4f);
    sample.setRadial(true);
    test_support::RecordingCanvas clipped;
    sample.paint(clipped);
    require(clipped.saves == clipped.restores && clipped.clips.size() == 2 &&
                clipped.gradients.size() == 1 && clipped.gradients.front().gradient.radial &&
                clipped.opacityLayers.size() == 1 && clipped.opacityLayers.front().opacity == .4f &&
                !clipped.strokeEllipses.empty(),
            "Drawing parameters did not reach Canvas or leaked state");
    sample.setClipped(false);
    sample.setCurve(1);
    test_support::RecordingCanvas unclipped;
    sample.paint(unclipped);
    require(unclipped.clips.size() == 1 && unclipped.saves == unclipped.restores,
            "Clipping toggle did not preserve the outer viewport clip");
}
} // namespace
int main() {
    try {
        drawingParameters();
        Application app({"org.oneui.tests.media", L"Media regression", QuitPolicy::Explicit});
        auto window = Window::create(L"Media regression", 1000, 900);
        require(bool(window), "Window creation failed");
        app.addWindow(*window, true);
        const lab::PageDescriptor descriptor{
            "features/drawing", lab::Category::Features, L"", L"", lab::PageKind::Drawing, 0, ""};
        auto page = lab::createMediaPage(descriptor, *window);
        auto root = page->widget();
        page->setTheme(true, false);
        window->setContent(root);
        window->initialize();
        window->prepareLayoutSnapshot();
        auto image = find<ImageView>(root);
        auto mode = find<Select>(root, L"图片缩放模式");
        auto horizontal = find<Select>(root, L"图片水平对齐");
        auto vertical = find<Select>(root, L"图片垂直对齐");
        auto radius = find<Slider>(root, L"图片圆角半径");
        require(image && mode && horizontal && vertical && radius && image->hasImage(),
                "Image controls are missing");
        next(*mode);
        next(*horizontal);
        next(*vertical);
        key(*radius, Key::End);
        require(image->contentMode() == ImageContentMode::Cover &&
                    image->horizontalContentAlignment() == ImageContentAlignment::End &&
                    image->verticalContentAlignment() == ImageContentAlignment::End &&
                    image->cornerRadius() == 60,
                "Image options were not applied");
        // Locate commands by their public text, then activate through keyboard handling.
        std::function<void(const std::shared_ptr<Widget> &, const wchar_t *)> click =
            [&](const std::shared_ptr<Widget> &widget, const wchar_t *text) {
                if (auto button = std::dynamic_pointer_cast<Button>(widget);
                    button && button->text() == text)
                    key(*button, Key::Enter);
                if (auto view = std::dynamic_pointer_cast<View>(widget))
                    for (const auto &child : view->children())
                        click(child, text);
            };
        click(root, L"清空图片");
        require(!image->hasImage(), "Clear image did not remove pixels");
        click(root, L"更新本地图片");
        require(image->hasImage() && image->cornerRadius() == 60, "Image update lost options");
        for (int i = 0; i < 30; ++i) {
            page->deactivate();
            page->setTheme(i % 2, i % 2);
            page->activate();
        }
        require(find<ImageView>(root) == image && image->contentMode() == ImageContentMode::Cover,
                "Theme changes recreated image state");
        window->show();
        std::uint64_t idlePaints = 0;
        bool passed = false;
        std::string failure;
        auto post = app.dispatcher();
        std::thread timer([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(600));
            post([&] { idlePaints = window->rendererInfo().paints; });
            std::this_thread::sleep_for(std::chrono::milliseconds(800));
            post([&] {
                if (!idlePaints || window->rendererInfo().paints != idlePaints)
                    failure = "Idle media page continued painting or no paint was observed";
                page->deactivate();
                window->setContent({});
                page.reset();
                passed = true;
                app.requestQuit();
            });
        });
        app.run();
        timer.join();
        require(passed && failure.empty(),
                failure.empty() ? "Media test did not finish" : failure.c_str());
        std::cout << "PASS: image alignment/mode/radius, clear/update, 30 retained theme cycles, "
                     "Canvas parameters and balanced state, actual idle paints=0.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

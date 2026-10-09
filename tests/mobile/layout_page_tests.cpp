#include "demo_page.hpp"
#include "oneui/mobile/widget_host.h"
#include "pages/page_factories.hpp"
#include "support/recording_canvas.h"
#include <iostream>
#include <stdexcept>

using namespace oneui;
namespace {
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class T> void collect(const std::shared_ptr<Widget> &root, std::vector<T *> &result) {
    if (auto typed = dynamic_cast<T *>(root.get()))
        result.push_back(typed);
    if (auto view = std::dynamic_pointer_cast<View>(root))
        for (const auto &child : view->children())
            collect(child, result);
}
} // namespace
int main() {
    try {
        auto page = lab::createBasicLayoutPage();
        auto original = page->widget();
        mobile::WidgetHost host;
        host.runtime().setViewport({1000, 1400, 1, 0, 0, 0, 0, 0});
        host.runtime().setSurface(true);
        host.runtime().setLifecycle(ONEUI_MOBILE_ACTIVE);
        host.setContent(original);
        test_support::RecordingCanvas canvas;
        require(host.paint(canvas, 1) == ONEUI_MOBILE_OK, "shared page first layout");
        std::vector<Button *> buttons;
        std::vector<Slider *> sliders;
        std::vector<Label *> labels;
        collect(original, buttons);
        collect(original, sliders);
        collect(original, labels);
        require(buttons.size() == 1 && sliders.size() == 2, "shared page controls present");
        const auto box = buttons[0]->frame();
        const float x = box.x + box.width / 2, y = box.y + box.height / 2;
        host.runtime().pushPointer({1, ONEUI_MOBILE_DOWN, ONEUI_MOBILE_TOUCH, x, y, 1, 1, 0});
        host.pumpInput();
        host.runtime().pushPointer({1, ONEUI_MOBILE_UP, ONEUI_MOBILE_TOUCH, x, y, 1, 2, 0});
        host.pumpInput();
        require(labels.front()->text().find(L"纵向") != std::wstring::npos,
                "direction command feedback");
        page->setTheme(false, false);
        host.paint(canvas, 2);
        page->setTheme(true, false);
        host.paint(canvas, 3);
        require(page->widget() == original &&
                    labels.front()->text().find(L"纵向") != std::wstring::npos,
                "theme retains page and direction state");
        require(sliders[0]->value() == 12 && sliders[1]->value() == 3,
                "default parameters preserved");
        std::vector<Grid *> grids;
        collect(original, grids);
        require(grids.size() == 1, "shared grid exists");
        sliders[0]->setValue(40);
        sliders[1]->setValue(4);
        host.paint(canvas, 4);
        const auto gridFrame = grids[0]->frame();
        require(gridFrame.height >= 164 && sliders[1]->frame().y >= gridFrame.y + gridFrame.height,
                "grid reserves rows and live gap before next control");
        for (const auto &child : grids[0]->children())
            require(child->frame().y + child->frame().height <=
                        gridFrame.y + gridFrame.height + .01f,
                    "grid children cannot overlap following content");
        sliders[1]->setValue(1);
        host.paint(canvas, 5);
        require(grids[0]->frame().height >= 572, "single-column grid reserves all six rows");
        host.setContent({});
        page.reset();
        require(!original->hasCompositionOwner(), "page detaches callbacks before release");
        auto controls = lab::createBasicControlsPage();
        host.setContent(controls->widget());
        host.paint(canvas, 6);
        buttons.clear();
        labels.clear();
        collect(controls->widget(), buttons);
        collect(controls->widget(), labels);
        require(buttons.size() == 2 && buttons[1]->disabled(),
                "shared control actions and disabled state");
        const auto action = buttons.front()->frame();
        const Point at{action.x + action.width / 2, action.y + action.height / 2};
        controls->widget()->onMouseDown({at, MouseButton::Left});
        controls->widget()->onMouseUp({at, MouseButton::Left});
        require(labels.front()->text().find(L"1 次") != std::wstring::npos,
                "shared control command executes");
        controls->setTheme(true, false);
        controls->setTheme(false, false);
        require(labels.front()->text().find(L"1 次") != std::wstring::npos,
                "shared controls retain state across themes");
        host.setContent({});
        std::cout << "shared desktop/mobile basic layout: construction, input, feedback, themes "
                     "and ownership passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

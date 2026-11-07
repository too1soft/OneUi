#include <iostream>
#include <oneui/controls/button.h>
#include <oneui/controls/popup.h>
#include <oneui/layout/overlay_host.h>
#include <stdexcept>

using namespace oneui;
namespace {
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
class PointerProbe final : public Widget {
  public:
    std::function<void()> down, up, wheel;
    void paint(Canvas &) override {}
    bool onMouseDown(const MouseEvent &) override {
        auto action = down;
        if (action)
            action();
        return true;
    }
    bool onMouseUp(const MouseEvent &) override {
        auto action = up;
        if (action)
            action();
        return true;
    }
    bool onMouseWheel(const MouseWheelEvent &) override {
        auto action = wheel;
        if (action)
            action();
        return false;
    }
    bool isFocusable() const override { return true; }
};
MouseEvent press() {
    MouseEvent event;
    event.position = {20, 20};
    event.button = MouseButton::Left;
    return event;
}
void closeRemovesPopup(bool destroyHost) {
    auto host = std::make_unique<OverlayHost>();
    host->setFrame({0, 0, 400, 300});
    auto popup = std::make_shared<Popup>();
    popup->setFrame(host->frame());
    popup->setContent(std::make_shared<Button>(L"Inside"));
    popup->setAnchorRect(Rect{200, 100, 40, 30});
    popup->setOpen(true);
    std::weak_ptr<Popup> weak = popup;
    popup->setOnClosed([&host, address = popup.get(), destroyHost] {
        if (destroyHost)
            host.reset();
        else
            host->removeOverlay(address);
    });
    host->addOverlay(popup);
    popup.reset();
    require(host->onMouseDown(press()), "Outside click was not consumed");
    require(weak.expired(), "Dispatch retained closed popup");
    if (host) {
        require(host->overlays().empty() && !host->activeFocusChild(), "Removed popup refocused");
        host->onMouseUp(press());
    }
}
void releaseRemovesItsOverlay() {
    OverlayHost host;
    auto probe = std::make_shared<PointerProbe>();
    probe->setFrame({0, 0, 100, 100});
    std::weak_ptr<PointerProbe> weak = probe;
    bool retainedDuringCallback = false;
    probe->up = [&] {
        host.clearOverlays();
        retainedDuringCallback = !weak.expired();
    };
    host.addOverlay(probe);
    probe.reset();
    host.onMouseDown(press());
    host.onMouseUp(press());
    require(retainedDuringCallback && weak.expired(), "Release did not retain callback receiver");
}
void wheelMutatesOverlayOrder() {
    OverlayHost host;
    auto upper = std::make_shared<PointerProbe>(), middle = std::make_shared<PointerProbe>(),
         lower = std::make_shared<PointerProbe>();
    for (auto &probe : {lower, middle, upper})
        probe->setFrame({0, 0, 100, 100});
    int lowerCalls = 0, removedCalls = 0;
    lower->wheel = [&] { ++lowerCalls; };
    middle->wheel = [&] { ++removedCalls; };
    upper->wheel = [&] {
        host.removeOverlay(middle.get());
        host.removeOverlay(upper.get());
    };
    host.addOverlay(lower, 1);
    host.addOverlay(middle, 2);
    host.addOverlay(upper, 3);
    MouseWheelEvent event;
    event.position = {20, 20};
    host.onMouseWheel(event);
    require(lowerCalls == 1 && removedCalls == 0, "Wheel dispatched stale overlay order");
}
void contentDestroysHost() {
    auto host = std::make_unique<OverlayHost>();
    auto content = std::make_shared<PointerProbe>();
    content->setFrame({0, 0, 100, 100});
    content->down = [&] { host.reset(); };
    host->setContent(content);
    host->onMouseDown(press());
    require(!host, "Content callback did not destroy its host");
}
class FocusProbe final : public Widget {
  public:
    FocusProbe(int &losses, bool &destroyedFocused)
        : losses_(losses), destroyedFocused_(destroyedFocused) {}
    ~FocusProbe() override { destroyedFocused_ = focused(); }
    void paint(Canvas &) override {}
    bool isFocusable() const override { return true; }
    bool onFocusChanged(bool focused) override {
        const bool changed = Widget::onFocusChanged(focused);
        if (changed && !focused)
            ++losses_;
        return changed;
    }
  private:
    int &losses_;
    bool &destroyedFocused_;
};
void destructionReleasesContentFocus(bool retainContent) {
    int losses = 0, invalidations = 0;
    bool destroyedFocused = false;
    auto host = std::make_unique<OverlayHost>();
    auto content = std::make_shared<FocusProbe>(losses, destroyedFocused);
    host->setContent(content);
    host->setInvalidator([&] { ++invalidations; });
    require(host->focusFirstLeaf() && content->focused(), "Content did not acquire inherited focus");
    if (!retainContent)
        content.reset();
    const int before = invalidations;
    host.reset();
    require(losses == 1, "Content focus was not released before its owner was destroyed");
    require(!destroyedFocused, "Content was destroyed while still focused");
    require(invalidations == before, "Host teardown invalidated its ancestor");
    if (content) {
        require(!content->focused(), "Retained content kept focus after host destruction");
        content->setVisible(false);
        require(invalidations == before, "Retained content called its disposed host");
    }
}
} // namespace
int main() {
    try {
        closeRemovesPopup(false);
        closeRemovesPopup(true);
        releaseRemovesItsOverlay();
        wheelMutatesOverlayOrder();
        contentDestroysHost();
        destructionReleasesContentFocus(false);
        destructionReleasesContentFocus(true);
        std::cout << "Overlay lifetime tests passed (self-removal, owner destruction, release "
                     "retention, wheel mutation).\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

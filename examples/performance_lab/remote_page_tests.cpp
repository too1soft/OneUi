#include "pages/page_factories.hpp"
#include <chrono>
#include <stdexcept>
#include <thread>

using namespace oneui;
namespace {
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class T> std::shared_ptr<T> find(const std::shared_ptr<Widget> &widget) {
    if (!widget)
        return {};
    if (auto value = std::dynamic_pointer_cast<T>(widget))
        return value;
    if (auto host = std::dynamic_pointer_cast<OverlayHost>(widget)) {
        if (auto value = find<T>(host->content()))
            return value;
        for (const auto &entry : host->overlays())
            if (auto value = find<T>(entry.child))
                return value;
    }
    if (auto view = std::dynamic_pointer_cast<View>(widget))
        for (auto &child : view->children())
            if (auto value = find<T>(child))
                return value;
    return {};
}
} // namespace
void remotePageLifecycle() {
    Application app(
        {"org.oneui.tests.remote-page", L"Remote page lifecycle", QuitPolicy::Explicit});
    auto window = Window::create(L"Remote page lifecycle", 1000, 900);
    app.addWindow(*window, true);
    auto page = lab::createRemotePage(*window);
    page->setTheme(true, false);
    window->setContent(page->widget());
    window->initialize();
    window->show();
    page->activate();
    auto frame = find<RealtimeFrameView>(page->widget());
    auto input = find<RemoteInputRegion>(page->widget());
    auto play = find<Button>(page->widget());
    require(frame && input && play && play->text() == L"播放本地模拟帧",
            "Remote page controls missing");
    const auto first = frame->latestFrame()->frameId;
    std::uint64_t stopped = 0, idlePaints = 0;
    bool passed = false;
    std::string failure;
    auto guard = [&](std::function<void()> action) {
        if (!failure.empty())
            return;
        try {
            action();
        } catch (const std::exception &error) {
            failure = error.what();
        }
    };
    app.post([&] {
        guard([&] {
            window->prepareLayoutSnapshot();
            require(window->requestFocus(play.get()), "Remote play focus failed");
            KeyEvent enter;
            enter.key = Key::Enter;
            require(play->onKeyDown(enter), "Remote play key action failed");
        });
    });
    auto post = app.dispatcher();
    std::thread timer([&] {
        auto after = [&](int milliseconds, std::function<void()> action) {
            std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
            post([&, action = std::move(action)] { guard(action); });
        };
        after(700, [&] {
            stopped = frame->latestFrame()->frameId;
            require(stopped > first, "Requested playback did not produce frames");
            page->deactivate();
            window->setContent(std::make_shared<Panel>());
        });
        after(400, [&] { idlePaints = window->rendererInfo().paints; });
        after(700, [&] {
            require(frame->latestFrame()->frameId == stopped, "Hidden page produced frames");
            require(idlePaints > 0 && window->rendererInfo().paints == idlePaints,
                    "Hidden playback kept repainting");
            window->setContent(page->widget());
            page->setTheme(false, true);
            page->activate();
            require(frame == find<RealtimeFrameView>(page->widget()), "Return rebuilt frame view");
            require(play->text() == L"播放本地模拟帧", "Playback resumed without user action");
        });
        after(400, [&] {
            require(frame->latestFrame()->frameId == stopped, "Return restarted frame production");
            KeyEvent enter;
            enter.key = Key::Enter;
            for (int i = 0; i < 30; ++i) {
                play->onKeyDown(enter);
                page->deactivate();
                page->activate();
            }
            page->deactivate();
            window->setContent({});
            page.reset(); // Pending animation callbacks must only inspect weak lifetime first.
        });
        after(300, [&] {
            require(frame->latestFrame()->frameId == stopped,
                    "Destroyed page callback produced a frame");
            passed = true;
        });
        post([&] { app.requestQuit(); });
    });
    app.run();
    timer.join();
    if (!failure.empty() || !passed)
        throw std::runtime_error(failure.empty() ? "Remote lifecycle did not finish" : failure);
}

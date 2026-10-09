#include "pages/page_factories.hpp"
#include "popup_showcase.hpp"
#include <chrono>
#include <iostream>
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
    if (auto item = std::dynamic_pointer_cast<T>(root)) {
        bool match = !*name || item->accessibleName() == name;
        if (auto button = std::dynamic_pointer_cast<Button>(root))
            match |= button->text() == name;
        if (match)
            return item;
    }
    if (auto popup = std::dynamic_pointer_cast<Popup>(root))
        if (auto item = find<T>(popup->content(), name))
            return item;
    if (auto host = std::dynamic_pointer_cast<OverlayHost>(root)) {
        if (auto item = find<T>(host->content(), name))
            return item;
        for (const auto &entry : host->overlays())
            if (auto item = find<T>(entry.child, name))
                return item;
    }
    if (auto view = std::dynamic_pointer_cast<View>(root))
        for (const auto &child : view->children())
            if (auto item = find<T>(child, name))
                return item;
    return {};
}
void key(Widget &widget, Key value) {
    KeyEvent event;
    event.key = value;
    require(widget.onKeyDown(event), "Keyboard action was not handled");
}
void select(Select &widget, int index) {
    key(widget, Key::Enter);
    key(widget, Key::Home);
    for (int i = 0; i < index; ++i)
        key(widget, Key::Down);
    key(widget, Key::Enter);
}
void click(Widget &root, const Widget &widget) {
    auto f = widget.frame();
    MouseEvent event;
    event.position = {f.x + f.width / 2, f.y + f.height / 2};
    event.button = MouseButton::Left;
    root.onMouseDown(event);
    root.onMouseUp(event);
}
// Regression for outside-close callbacks removing their own overlay while
// OverlayHost dispatches pointer input. No retained Popup masks its destruction.
void selfRemovingOverlay() {
    auto host = std::make_shared<OverlayHost>();
    host->setFrame({0, 0, 600, 400});
    auto popup = std::make_shared<Popup>();
    popup->setFrame(host->frame());
    popup->setContent(std::make_shared<Button>(L"Inside"));
    popup->setAnchorRect(Rect{10, 10, 40, 30});
    popup->setOpen(true);
    std::weak_ptr<Popup> weakPopup = popup;
    popup->setOnClosed([weakHost = std::weak_ptr<OverlayHost>(host), address = popup.get()] {
        if (auto h = weakHost.lock())
            h->removeOverlay(address);
    });
    host->addOverlay(popup);
    popup.reset();
    MouseEvent event;
    event.position = {550, 350};
    event.button = MouseButton::Left;
    require(host->onMouseDown(event), "Outside close was not consumed");
    require(host->overlays().empty() && weakPopup.expired() && !host->activeFocusChild(),
            "Removed popup was retained or refocused by event dispatch");
    host->onMouseUp(event);
}
} // namespace
int main() {
    try {
        selfRemovingOverlay();
        Application app({"org.oneui.tests.popups", L"Popup regression", QuitPolicy::Explicit});
        auto window = Window::create(L"Popup regression", 1000, 900);
        require(bool(window), "Window creation failed");
        app.addWindow(*window, true);
        auto page = lab::createNotificationsPage();
        page->setTheme(true, false);
        page->activate();
        auto root = page->widget();
        window->setContent(root);
        window->initialize();
        window->prepareLayoutSnapshot();
        auto host = find<lab::PopupShowcase>(root);
        auto trigger = find<Button>(root, L"打开弹层");
        auto background = find<Button>(root, L"外部按钮：测试是否收到点击");
        auto mode = find<Select>(root, L"弹层交互模式");
        auto feedback = find<Label>(root, L"弹层反馈");
        require(host && trigger && background && mode && feedback, "Popup controls missing");
        key(*trigger, Key::Enter);
        window->prepareLayoutSnapshot();
        auto popup = find<Popup>(host);
        auto editor = find<TextField>(host, L"弹层草稿");
        require(popup && editor && popup->isOpen(), "Popup did not open");
        require(popup->resolvedContentRect().y < trigger->frame().y,
                "Bottom placement did not flip away from the lower viewport edge");
        editor->setText(L"Retained draft");
        editor->setSelectionRange(2, 7);
        click(*host, *background);
        require(host->overlays().empty() && !popup->isOpen(), "Light dismiss did not close");
        require(feedback->text().find(L"收到") == std::wstring::npos,
                "Light dismiss click leaked into background action");
        require(trigger->focused(), "Closing did not restore trigger focus");

        select(*mode, 0);
        key(*trigger, Key::Enter);
        window->prepareLayoutSnapshot();
        click(*host, *background);
        require(popup->isOpen() && feedback->text().find(L"收到 1") != std::wstring::npos,
                "Modeless popup blocked the background action");
        key(*host, Key::Escape);
        select(*mode, 2);
        key(*trigger, Key::Enter);
        window->prepareLayoutSnapshot();
        const auto before = feedback->text();
        click(*host, *background);
        require(popup->isOpen() && feedback->text() == before &&
                    !host->requestFocus(background.get()),
                "Modal popup leaked input/focus");
        auto nested = find<Button>(host, L"打开第二层确认");
        key(*nested, Key::Enter);
        require(host->overlays().size() == 2 && !host->requestFocus(editor.get()),
                "Second modal did not constrain focus");
        window->prepareLayoutSnapshot();
        key(*host, Key::Escape);
        require(host->overlays().size() == 1 && popup->isOpen() && editor->focused(),
                "Escape did not close only the top layer and restore draft focus");
        require(editor->text() == L"Retained draft" && editor->selectionStart() == 2 &&
                    editor->selectionEnd() == 7,
                "Nested dialog lost edit state");
        key(*host, Key::Escape);
        auto escape = find<Checkbox>(host, L"允许 Esc 关闭");
        auto controlled = find<Checkbox>(host, L"绑定弹层打开状态");
        require(escape && controlled, "Popup policy switches missing");
        key(*escape, Key::Space);
        key(*controlled, Key::Space);
        KeyEvent esc;
        esc.key = Key::Escape;
        host->onKeyDown(esc);
        require(popup->isOpen() && controlled->checked(),
                "Disabled Escape closed controlled popup");
        key(*find<Button>(host, L"关闭弹层并保留草稿"), Key::Enter);
        require(!controlled->checked() && host->overlays().empty(),
                "Close did not update bound state");
        key(*controlled, Key::Space);
        key(*controlled, Key::Space);
        require(host->overlays().empty(), "External state update retained a modal trap");
        key(*escape, Key::Space);
        for (int i = 0; i < 30; ++i) {
            page->deactivate();
            require(host->overlays().empty() && !popup->isOpen(), "Hidden page retained overlays");
            page->setTheme(i % 2, i % 2);
            page->activate();
            key(*trigger, Key::Enter);
            require(find<TextField>(host) == editor, "Page cycles recreated the editor");
        }
        page->deactivate();
        key(*trigger, Key::Enter);
        require(host->overlays().empty(), "Inactive page callback opened an overlay");
        page->activate();
        window->show();
        std::uint64_t idle = 0;
        bool passed = false;
        std::string failure;
        auto post = app.dispatcher();
        std::thread timer([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(650));
            post([&] { idle = window->rendererInfo().paints; });
            std::this_thread::sleep_for(std::chrono::milliseconds(800));
            post([&] {
                if (!idle || window->rendererInfo().paints != idle)
                    failure = "Idle popup page continued painting or never painted";
                page->deactivate();
                window->setContent({});
                auto toast = find<Button>(root, L"追加 Toast");
                page.reset();
                key(*trigger, Key::Enter);
                key(*toast, Key::Enter);
                if (!host->overlays().empty())
                    failure = "Destroyed page callback remained active";
                passed = true;
                app.requestQuit();
            });
        });
        app.run();
        timer.join();
        require(passed && failure.empty(),
                failure.empty() ? "Test did not complete" : failure.c_str());
        std::cout
            << "PASS: popup outside-click policies, automatic flip, nested Escape/focus, "
               "30 retained edit/lifecycle cycles, stale callbacks, self-removal, idle paints=0.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

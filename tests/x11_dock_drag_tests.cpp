#include "oneui/controls/tabs.h"
#include "oneui/controls/text_field.h"
#include "oneui/layout/dock_session.h"
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#undef None
#undef Status
#include <chrono>
#include <iostream>
#include <thread>

using namespace oneui;
namespace {
void pause() { std::this_thread::sleep_for(std::chrono::milliseconds(200)); }
Tabs *tabs(const std::shared_ptr<Widget> &widget) {
    if (auto result = dynamic_cast<Tabs *>(widget.get()))
        return result;
    if (auto view = dynamic_cast<View *>(widget.get()))
        for (auto &child : view->children())
            if (auto result = tabs(child))
                return result;
    return nullptr;
}
bool exercise(bool cancel) {
    Application app({cancel ? "org.oneui.tests.xdragcancel" : "org.oneui.tests.xdragdrop",
                     L"Xdnd test", QuitPolicy::Explicit});
    auto source = oneui::Window::create(L"OneUI Xdnd test source", 480, 360);
    auto target = oneui::Window::create(L"OneUI Xdnd test target", 480, 360);
    auto left = std::make_shared<Panel>(), right = std::make_shared<Panel>();
    source->setContent(left);
    target->setContent(right);
    source->initialize();
    target->initialize();
    app.addWindow(*source);
    app.addWindow(*target);
    DockSession dock(app);
    auto a = dock.attachHost(*source, left), b = dock.attachHost(*target, right);
    if (!dock.nativeDragSupported(a) || !dock.nativeDragSupported(b))
        return false;
    auto editor = std::make_shared<TextField>();
    editor->setText(L"Original Xdnd document");
    dock.addPane({1, L"Drag this tab", editor, editor}, {a});
    auto display = XOpenDisplay(nullptr);
    if (!display)
        return false;
    auto nativeSource = reinterpret_cast<std::uintptr_t>(source->nativeHandle());
    auto nativeTarget = reinterpret_cast<std::uintptr_t>(target->nativeHandle());
    source->show();
    target->show();
    source->activate();
    XMoveResizeWindow(display, nativeSource, 60, 80, 480, 360);
    XMoveResizeWindow(display, nativeTarget, 620, 80, 480, 360);
    XSync(display, False);
    source->prepareLayoutSnapshot();
    target->prepareLayoutSnapshot();
    auto strip = tabs(left);
    if (!strip) {
        XCloseDisplay(display);
        return false;
    }
    auto item = strip->itemFrame(0);
    const float dpi = source->dpiScale();
    bool passed = false;
    std::thread input([&, item] {
        auto d = XOpenDisplay(nullptr);
        if (!d) {
            app.post([&] { app.requestQuit(); });
            return;
        }
        pause();
        pause();
        ::Window child = 0, root = DefaultRootWindow(d);
        int x = 0, y = 0, endX = 0, endY = 0;
        XTranslateCoordinates(d, nativeSource, root, int((item.x + item.width / 2) * dpi),
                              int((item.y + item.height / 2) * dpi), &x, &y, &child);
        XTranslateCoordinates(d, nativeTarget, root, 240, 180, &endX, &endY, &child);
        std::cerr << "Pointer test from=" << x << "," << y << " to=" << endX << "," << endY
                  << " tab=" << item.width << "x" << item.height << "\n";
        XRaiseWindow(d, nativeSource);
        XFlush(d);
        pause();
        XTestFakeMotionEvent(d, -1, x, y, CurrentTime);
        XFlush(d);
        pause();
        XTestFakeButtonEvent(d, 1, True, CurrentTime);
        XFlush(d);
        pause();
        XTestFakeMotionEvent(d, -1, x, y + 70, CurrentTime);
        XFlush(d);
        pause();
        XRaiseWindow(d, nativeTarget);
        XFlush(d);
        pause();
        XTestFakeMotionEvent(d, -1, endX, endY, CurrentTime);
        XFlush(d);
        pause();
        pause();
        if (cancel) {
            auto key = XKeysymToKeycode(d, XK_Escape);
            XTestFakeKeyEvent(d, key, True, CurrentTime);
            XTestFakeKeyEvent(d, key, False, CurrentTime);
            XFlush(d);
            pause();
        }
        XTestFakeButtonEvent(d, 1, False, CurrentTime);
        XFlush(d);
        pause();
        pause();
        app.post([&] {
            passed = dock.location(1).host == (cancel ? a : b) && dock.content(1) == editor &&
                     editor->text() == L"Original Xdnd document" && dock.hosts().size() == 2;
            if (!passed)
                std::cerr << "Unexpected host=" << dock.location(1).host << " cancel=" << cancel
                          << '\n';
            app.requestQuit();
        });
        XCloseDisplay(d);
    });
    app.run();
    input.join();
    XCloseDisplay(display);
    return passed;
}
} // namespace
int main() {
    if (!XInitThreads())
        return 77;
    auto display = XOpenDisplay(nullptr);
    if (!display)
        return 77;
    int event = 0, error = 0, major = 0, minor = 0;
    if (!XTestQueryExtension(display, &event, &error, &major, &minor)) {
        XCloseDisplay(display);
        return 77;
    }
    ::Window root = 0, child = 0, focus = 0;
    int x = 0, y = 0, rx = 0, ry = 0, revert = 0;
    unsigned int mask = 0;
    XQueryPointer(display, DefaultRootWindow(display), &root, &child, &x, &y, &rx, &ry, &mask);
    XGetInputFocus(display, &focus, &revert);
    bool passed = false;
    try {
        passed = exercise(false) && exercise(true);
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
    }
    XTestFakeMotionEvent(display, -1, x, y, CurrentTime);
    if (focus)
        XSetInputFocus(display, focus, revert, CurrentTime);
    XCloseDisplay(display);
    std::cout << (passed ? "Native Xdnd pointer drop and Escape rollback passed\n"
                         : "Native Xdnd acceptance failed\n");
    return passed ? 0 : 1;
}

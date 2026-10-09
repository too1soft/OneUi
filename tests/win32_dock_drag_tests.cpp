#include "../src/platform/win32/drop_files_win32.h"
#include "oneui/controls/tabs.h"
#include "oneui/controls/text_field.h"
#include "oneui/layout/dock_session.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <windows.h>

using namespace oneui;
namespace {
void pause() { std::this_thread::sleep_for(std::chrono::milliseconds(140)); }
void button(DWORD flag) {
    INPUT event{};
    event.type = INPUT_MOUSE;
    event.mi.dwFlags = flag;
    SendInput(1, &event, sizeof(event));
}
void escape() {
    INPUT events[2]{};
    for (auto &e : events) {
        e.type = INPUT_KEYBOARD;
        e.ki.wVk = VK_ESCAPE;
    }
    events[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, events, sizeof(INPUT));
}
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
    Application app({cancel ? "org.oneui.tests.dragcancel" : "org.oneui.tests.dragdrop",
                     L"Native drag acceptance", QuitPolicy::Explicit});
    auto source = Window::create(L"OneUI native drag test · source", 480, 360);
    auto target = Window::create(L"OneUI native drag test · target", 480, 360);
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
    editor->setText(L"Original native drag document");
    dock.addPane({1, L"Drag this tab", editor, editor}, {a});
    const auto sourceHwnd = static_cast<HWND>(source->nativeHandle()),
               targetHwnd = static_cast<HWND>(target->nativeHandle());
    SetWindowPos(sourceHwnd, nullptr, 60, 60, 500, 400, SWP_NOZORDER);
    SetWindowPos(targetHwnd, nullptr, 610, 60, 500, 400, SWP_NOZORDER);
    source->show();
    target->show();
    source->activate();
    source->prepareLayoutSnapshot();
    target->prepareLayoutSnapshot();
    auto strip = tabs(left);
    if (!strip)
        return false;
    auto item = strip->itemFrame(0);
    POINT start{LONG((item.x + item.width / 2) * source->dpiScale()),
                LONG((item.y + item.height / 2) * source->dpiScale())};
    ClientToScreen(sourceHwnd, &start);
    POINT end{LONG(target->clientPixelSize().width / 2),
              LONG(target->clientPixelSize().height / 2)};
    ClientToScreen(targetHwnd, &end);
    bool passed = false;
    std::thread input([&] {
        pause();
        SetCursorPos(start.x, start.y);
        pause();
        button(MOUSEEVENTF_LEFTDOWN);
        pause();
        SetCursorPos(start.x, start.y + 70);
        pause();
        SetCursorPos(end.x, end.y);
        pause();
        if (cancel)
            escape();
        button(MOUSEEVENTF_LEFTUP);
        pause();
        pause();
        // A failed native grab must never leave the suite stuck in an OLE loop.
        escape();
        button(MOUSEEVENTF_LEFTUP);
        app.post([&] {
            const auto location = dock.location(1);
            passed = location.host == (cancel ? a : b) && dock.content(1) == editor &&
                     editor->text() == L"Original native drag document" && dock.hosts().size() == 2;
            if (!passed)
                std::cerr << "Unexpected host=" << location.host << " cancel=" << cancel << '\n';
            app.requestQuit();
        });
    });
    app.run();
    input.join();
    return passed;
}
} // namespace
int main() {
    POINT pointer{};
    GetCursorPos(&pointer);
    auto foreground = GetForegroundWindow();
    struct Payload {
        DROPFILES header{};
        wchar_t text[8]{};
    } data;
    data.header.pFiles = sizeof(DROPFILES);
    data.header.fWide = TRUE;
    if (!oneui::platform::validDropFiles(&data.header, sizeof(data)))
        return 1;
    data.header.pFiles = sizeof(data) + 1;
    if (oneui::platform::validDropFiles(&data.header, sizeof(data)))
        return 1;
    data.header.pFiles = sizeof(DROPFILES);
    for (auto &ch : data.text)
        ch = L'x';
    if (oneui::platform::validDropFiles(&data.header, sizeof(data)))
        return 1;
    bool passed = false;
    try {
        passed = exercise(false) && exercise(true);
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
    }
    SetCursorPos(pointer.x, pointer.y);
    if (IsWindow(foreground))
        SetForegroundWindow(foreground);
    std::cout << (passed ? "Native OLE mouse drop and Escape rollback passed\n"
                         : "Native OLE acceptance failed\n");
    return passed ? 0 : 1;
}

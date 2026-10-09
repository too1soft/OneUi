#include "oneui/controls/tabs.h"
#include "oneui/controls/text_field.h"
#include "oneui/layout/dock_session.h"
#import <Cocoa/Cocoa.h>
#include <chrono>
#include <iostream>
#include <string_view>
#include <thread>

using namespace oneui;
namespace {
void pause() { std::this_thread::sleep_for(std::chrono::milliseconds(200)); }
void mouse(CGEventType type, CGPoint point) {
    auto event = CGEventCreateMouseEvent(nullptr, type, point, kCGMouseButtonLeft);
    CGEventPost(kCGHIDEventTap, event);
    CFRelease(event);
}
void escape() {
    for (bool down : {true, false}) {
        auto event = CGEventCreateKeyboardEvent(nullptr, 53, down);
        CGEventPost(kCGHIDEventTap, event);
        CFRelease(event);
    }
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
CGPoint screenPoint(NSView *view, NSPoint point) {
    point = [view convertPoint:point toView:nil];
    point = [view.window convertPointToScreen:point];
    return {point.x, NSScreen.screens.firstObject.frame.size.height - point.y};
}
enum class Gesture { Merge, Cancel, Split, Detach };
bool exercise(Gesture gesture) {
    const bool cancel = gesture == Gesture::Cancel;
    Application app({"org.oneui.tests.cocoadrag", L"AppKit drag test", QuitPolicy::Explicit});
    auto source = Window::create(L"OneUI AppKit drag source", 440, 330);
    auto target = Window::create(L"OneUI AppKit drag target", 440, 330);
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
    editor->setText(L"Original AppKit document");
    editor->setSelectionRange(9, 15);
    int prepared = 0, committed = 0;
    DockPane pane{1, L"Drag this tab", editor, editor};
    pane.transfer = [&](Window &, Window &, DockTransferPhase phase) {
        prepared += phase == DockTransferPhase::Prepare;
        committed += phase == DockTransferPhase::Commit;
        return true;
    };
    dock.addPane(std::move(pane), {a});
    auto resident = std::make_shared<TextField>();
    resident->setText(L"Target document");
    dock.addPane({2, L"Target tab", resident, resident}, {b});
    auto nativeSource = (__bridge NSWindow *)source->nativeHandle();
    auto nativeTarget = (__bridge NSWindow *)target->nativeHandle();
    auto bounds = NSScreen.mainScreen.visibleFrame;
    [nativeSource setFrameOrigin:NSMakePoint(bounds.origin.x + 30,
                                             bounds.origin.y + bounds.size.height - 430)];
    [nativeTarget setFrameOrigin:NSMakePoint(bounds.origin.x + 520,
                                             bounds.origin.y + bounds.size.height - 430)];
    source->show();
    target->show();
    source->activate();
    source->prepareLayoutSnapshot();
    target->prepareLayoutSnapshot();
    auto strip = tabs(left);
    if (!strip)
        return false;
    auto item = strip->itemFrame(0);
    auto start = screenPoint(nativeSource.contentView,
                             NSMakePoint(item.x + item.width / 2, item.y + item.height / 2));
    auto end = screenPoint(nativeTarget.contentView, NSMakePoint(220, 165));
    if (gesture == Gesture::Split)
        end = screenPoint(nativeTarget.contentView, NSMakePoint(35, 165));
    if (gesture == Gesture::Detach)
        end = screenPoint(nativeSource.contentView, NSMakePoint(220, 420));
    auto pasteboard = [NSPasteboard pasteboardWithName:NSPasteboardNameDrag];
    const auto previousPasteboard = pasteboard.changeCount;
    bool passed = false;
    std::thread input([&] {
        @autoreleasepool {
            pause();
            pause();
            mouse(kCGEventMouseMoved, start);
            pause();
            mouse(kCGEventLeftMouseDown, start);
            pause();
            mouse(kCGEventLeftMouseDragged, {start.x, start.y + 70});
            pause();
            mouse(kCGEventLeftMouseDragged, end);
            pause();
            pause();
            if (cancel) {
                escape();
                pause();
            }
            mouse(kCGEventLeftMouseUp, end);
            pause();
            pause();
            app.post([&] {
                const auto location = dock.location(1);
                const bool detached = gesture == Gesture::Detach;
                const bool split = gesture == Gesture::Split;
                const bool expectedHost = detached ? location.host != a && location.host != b
                                                   : location.host == (cancel ? a : b);
                const bool expectedGroup = cancel || detached ||
                                           (split ? location.group != dock.location(2).group
                                                  : location.group == dock.location(2).group);
                passed = expectedHost && expectedGroup && dock.content(1) == editor &&
                         dock.content(2) == resident && resident->text() == L"Target document" &&
                         editor->text() == L"Original AppKit document" &&
                         editor->selectedText() == L"AppKit" &&
                         dock.hosts().size() == (detached ? 3u : 2u) &&
                         prepared == (cancel ? 0 : 1) && committed == (cancel ? 0 : 1) &&
                         pasteboard.changeCount > previousPasteboard &&
                         [pasteboard.types containsObject:@"org.oneui.dock-v1"];
                if (!passed)
                    std::cerr << "Unexpected host=" << location.host << " gesture=" << int(gesture)
                              << " groups=" << location.group << '/' << dock.location(2).group
                              << " transfers=" << prepared << '/' << committed
                              << " pasteboard=" << (pasteboard.changeCount > previousPasteboard)
                              << " selection=" << editor->selectedText().size() << '\n';
                std::cout << "AppKit gesture " << int(gesture)
                          << (passed ? " passed\n" : " failed\n");
                app.requestQuit();
            });
        }
    });
    app.run();
    input.join();
    return passed;
}
} // namespace
int main(int argc, char **argv) {
    @autoreleasepool {
        if (argc == 2 && std::string_view(argv[1]) == "--request-input-access") {
            const bool granted = CGPreflightPostEventAccess() || CGRequestPostEventAccess();
            std::cout << (granted ? "Automated input access is granted.\n"
                                  : "Grant the requested Accessibility permission in System "
                                    "Settings, then run this test again.\n");
            return granted ? 0 : 77;
        }
        if (argc != 1) {
            std::cerr << "Usage: oneui_macos_dock_drag_tests [--request-input-access]\n";
            return 2;
        }
        if (!CGPreflightPostEventAccess()) {
            std::cout << "SKIP: macOS has not granted automated input access; native gesture "
                         "acceptance requires manual verification.\n";
            return 77;
        }
        auto event = CGEventCreate(nullptr);
        auto position = CGEventGetLocation(event);
        CFRelease(event);
        auto foreground = NSWorkspace.sharedWorkspace.frontmostApplication;
        bool passed = false;
        try {
            passed = exercise(Gesture::Merge) && exercise(Gesture::Cancel) &&
                     exercise(Gesture::Split) && exercise(Gesture::Detach);
        } catch (const std::exception &error) {
            std::cerr << error.what() << '\n';
        }
        mouse(kCGEventMouseMoved, position);
        [foreground activateWithOptions:NSApplicationActivateIgnoringOtherApps];
        std::cout << (passed
                          ? "Native AppKit merge, Escape rollback, edge split and detach passed\n"
                          : "Native AppKit acceptance failed\n");
        return passed ? 0 : 1;
    }
}

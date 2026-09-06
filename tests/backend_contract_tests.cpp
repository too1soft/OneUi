#include "oneui/controls/text_field.h"
#include "oneui/layout/panel.h"
#include "oneui/layout/stack.h"
#include "oneui/platform/window.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

namespace {

int failures = 0;

void expectTrue(const char* name, bool value) {
    if (!value) {
        std::cerr << name << ": expected true\n";
        ++failures;
    }
}

void expectNear(const char* name, float actual, float expected, float tolerance) {
    const float diff = actual > expected ? actual - expected : expected - actual;
    if (diff > tolerance) {
        std::cerr << name << ": expected " << expected << ", actual " << actual << "\n";
        ++failures;
    }
}

void testHiddenWindowLifecycleAndStateContract() {
    oneui::WindowOptions options;
    options.title = L"OneUI Backend Contract Smoke";
    options.width = 640;
    options.height = 420;
    options.visible = false;
    options.borderless = true;
    options.resizable = true;

    auto window = oneui::Window::create(std::move(options));
    expectTrue("Window create", window != nullptr);
    if (!window) {
        return;
    }

    const oneui::Size initialSize = window->clientSize();
    expectNear("Initial client width before native create", initialSize.width, 640.0f, 0.01f);
    expectNear("Initial client height before native create", initialSize.height, 420.0f, 0.01f);
    expectTrue("Window DPI scale is positive", window->dpiScale() > 0.0f);
    const oneui::Size initialPixelSize = window->clientPixelSize();
    expectTrue("Initial pixel width is positive", initialPixelSize.width > 0.0f);
    expectTrue("Initial pixel height is positive", initialPixelSize.height > 0.0f);

    auto root = std::make_shared<oneui::Stack>(oneui::StackDirection::Column);
    auto field = std::make_shared<oneui::TextField>(L"Backend smoke text field");
    auto panel = std::make_shared<oneui::Panel>();
    root->add(field);
    root->add(panel);
    window->setContent(root);

    window->setTitle(L"OneUI Backend Contract Smoke Updated");
    window->setBorderless(false);
    window->setBorderless(true);
    window->setFullscreen(true);
    const oneui::Size fullscreenSize = window->clientSize();
    const oneui::Size fullscreenPixelSize = window->clientPixelSize();
    expectTrue("Fullscreen client width is positive", fullscreenSize.width > 0.0f);
    expectTrue("Fullscreen client height is positive", fullscreenSize.height > 0.0f);
    expectTrue("Fullscreen pixel width is positive", fullscreenPixelSize.width > 0.0f);
    expectTrue("Fullscreen pixel height is positive", fullscreenPixelSize.height > 0.0f);
    expectNear(
        "Fullscreen logical to physical width relation",
        fullscreenSize.width * window->dpiScale(),
        fullscreenPixelSize.width,
        2.0f);
    expectNear(
        "Fullscreen logical to physical height relation",
        fullscreenSize.height * window->dpiScale(),
        fullscreenPixelSize.height,
        2.0f);
    window->setFullscreen(false);
    window->toggleMaximize();
    window->toggleMaximize();
    window->minimize();
    window->requestRedraw();

    bool posted = false;
    window->post([&] {
        posted = true;
        window->close();
    });
    const int runResult = window->run();
    expectTrue("Window post callback ran", posted);
    expectTrue("Window run exits cleanly", runResult == 0);
}

void testAnimationFrameContract() {
    oneui::WindowOptions options;
    options.title = L"OneUI Animation Frame Smoke";
    options.width = 320;
    options.height = 240;
    options.visible = false;

    auto window = oneui::Window::create(std::move(options));
    expectTrue("Animation window create", window != nullptr);
    if (!window) {
        return;
    }

    bool frameRan = false;
    double frameTime = 0.0;
    window->requestAnimationFrame([&](double nowMs) {
        frameRan = true;
        frameTime = nowMs;
        window->close();
    });
    const int runResult = window->run();
    expectTrue("Animation frame callback ran", frameRan);
    expectTrue("Animation frame timestamp is positive", frameTime > 0.0);
    expectTrue("Animation frame run exits cleanly", runResult == 0);
}

void testSystemClipboardRoundTrip() {
    oneui::SystemClipboard clipboard;
    const std::wstring previous = clipboard.text();

    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    const auto stamp = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
    const std::wstring value = L"OneUI clipboard smoke " + std::to_wstring(stamp);

    clipboard.setText(value);
    bool roundTrip = false;
    for (int attempt = 0; attempt < 8; ++attempt) {
        if (clipboard.text() == value) {
            roundTrip = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if (!roundTrip) {
        expectTrue("Advertised system clipboard round trip", false);
        return;
    }
    clipboard.setText(previous);
}

void testIndependentWindowLifetimes() {
    oneui::WindowOptions options;
    options.visible = false;
    auto first = oneui::Window::create(options);
    auto second = oneui::Window::create(options);
    first->initialize(); second->initialize();
    expectTrue("Backend identity is explicit", first->backend() != oneui::WindowBackend::Unknown);
    first->close();
    expectTrue("Closed window rejects posts", !first->post([] {}));
    bool cancelled = false, workerRan = false, frameRan = false;
    first->requestAnimationFrame([&](double) { cancelled = true; });
    std::thread worker([&] { second->post([&] { workerRan = true; }); });
    worker.join();
    second->requestAnimationFrame([&](double) { frameRan = true; second->close(); });
    second->run();
    expectTrue("Closing one window preserves another's queue", workerRan);
    expectTrue("Closing one window preserves another's animation", frameRan);
    expectTrue("Closed window never invokes queued frame", !cancelled);
}

void testRepeatedCreationAndCancellation() {
    for (int iteration = 0; iteration < 24; ++iteration) {
        oneui::WindowOptions options;
        options.visible = false;
        auto window = oneui::Window::create(options);
        window->initialize();
        bool cancelled = false;
        window->post([&] { window->close(); });
        window->post([&] { cancelled = true; });
        window->run();
        expectTrue("Closing cancels the remainder of a queued batch", !cancelled);
    }
}

void testCancelledCaptureCanReenterPost() {
    oneui::WindowOptions options;
    options.visible = false;
    auto window = oneui::Window::create(options);
    window->initialize();
    struct ReentrantCapture {
        oneui::Window* window;
        bool* released;
        ~ReentrantCapture() {
            *released = true;
            expectTrue("Capture destructor observes a closed queue", !window->post([] {}));
            window->requestAnimationFrame([](double) {});
        }
    };
    bool released = false;
    auto capture = std::make_shared<ReentrantCapture>();
    capture->window = window.get();
    capture->released = &released;
    window->post([capture] {});
    window->requestAnimationFrame([capture](double) {});
    capture.reset();
    window->close();
    expectTrue("Closing releases captured objects without holding the queue lock", released);
}

} // namespace

int main(int argc, char**) {
    if (argc > 1) {
        auto probe = oneui::Window::create(L"Clipboard capability probe", 320, 200);
        probe->initialize();
        if (!(probe->capabilities() & oneui::WindowCapabilityClipboard)) {
            std::cerr << "SKIP: clipboard requires an interactive seat/user-input serial; native acceptance remains pending.\n";
            return 77;
        }
        testSystemClipboardRoundTrip();
        return failures == 0 ? 0 : 1;
    }
    testHiddenWindowLifecycleAndStateContract();
    testAnimationFrameContract();
    testIndependentWindowLifetimes();
    testRepeatedCreationAndCancellation();
    testCancelledCaptureCanReenterPost();

    if (failures != 0) {
        std::cerr << failures << " backend contract smoke test(s) failed.\n";
        return 1;
    }

    return 0;
}

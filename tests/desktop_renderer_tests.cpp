#include "oneui/platform/window.h"
#include "oneui/platform/monitor.h"
#include "oneui/controls/text_field.h"
#include "platform/shared/desktop_window.h"
#include <iostream>
#include <string>
#ifdef ONEUI_TEST_EGL
#include <EGL/egl.h>
#endif
#ifdef ONEUI_TEST_X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

int main(int argc, char** argv) {
    const bool software = argc > 1 && std::string(argv[1]) == "software";
    int failures = 0;
    auto check = [&](bool value, const char* message) { if (!value) { std::cerr << message << '\n'; ++failures; } };
#ifdef ONEUI_TEST_EGL
    eglBindAPI(EGL_OPENGL_API); // The embedding application may use another EGL client API.
#endif
    // Two contexts on one display exercise display ownership: destroying one
    // must not terminate EGL while another window is still rendering.
    auto first = oneui::Window::create(L"Renderer lifecycle A", 240, 160);
    auto second = oneui::Window::create(L"Renderer lifecycle B", 240, 160);
    first->initialize(); second->initialize();
    first->show(); second->show();
    second->setContent(std::make_shared<oneui::TextField>(L"GPU \u4e2d\u6587 \U0001f600"));
#if ONEUI_DEVELOPER_TOOLS
    check(second->setDeveloperTools({true, false}), "Developer diagnostics unavailable");
#else
    check(!second->setDeveloperTools({true, false}), "Release unexpectedly enables developer tools");
#endif
    auto* desktop = dynamic_cast<oneui::platform::DesktopWindow*>(second.get());
    if (!desktop) return 1;
    auto waitForPaint = [&](std::uint64_t count) {
        const auto deadline = oneui::platform::DesktopWindow::nowMs() + 5000;
        while (second->rendererInfo().paints <= count && oneui::platform::DesktopWindow::nowMs() < deadline)
            desktop->pumpNativeEvents(10);
        check(second->rendererInfo().paints > count, "Native presentation timed out");
    };
    waitForPaint(0);
    auto before = second->rendererInfo();
    std::cout << "backend=" << static_cast<int>(before.backend) << " device=" << before.device
              << " reason=" << before.reason << " paints=" << before.paints << '\n';
    check(before.backend == (software ? oneui::RenderBackend::Software :
        second->backend() == oneui::WindowBackend::Cocoa ? oneui::RenderBackend::Metal : oneui::RenderBackend::OpenGL),
        "Requested native renderer was not established");
    check(!before.device.empty(), "Device diagnosis missing");
#ifdef ONEUI_TEST_EGL
    check(eglQueryAPI() == EGL_OPENGL_API, "OneUI changed the embedding application's EGL API");
#endif
#if ONEUI_DEVELOPER_TOOLS
    check(second->developerSnapshot().enabled && second->developerSnapshot().frames > 0, "Native paint diagnostics not recorded");
#else
    check(!second->developerSnapshot().enabled, "Release unexpectedly records developer diagnostics");
#endif
    const auto sizeBeforeZoom = second->clientSize();
    const auto pixelsBeforeZoom = second->clientPixelSize();
    second->setMinimumClientSize({80, 40});
    second->setContentScale(2);
    check(std::abs(second->clientSize().width * 2 - sizeBeforeZoom.width) < 1 &&
        second->clientPixelSize().width == pixelsBeforeZoom.width, "Content zoom changed the native backing size");
    first->close(); first.reset();
    second->requestRedraw();
    waitForPaint(before.paints);
    check(second->rendererInfo().backend == before.backend, "Closing another window lost the renderer");
#ifdef ONEUI_TEST_X11
    if (second->backend() == oneui::WindowBackend::X11) {
        auto* display = XOpenDisplay(nullptr);
        check(display != nullptr, "Cannot inspect X11 size constraints");
        if (display) {
            XSizeHints hints{};
            long supplied = 0;
            const auto native = static_cast<::Window>(reinterpret_cast<std::uintptr_t>(second->nativeHandle()));
            check(XGetWMNormalHints(display, native, &hints, &supplied) && (hints.flags & PMinSize) &&
                hints.min_width == static_cast<int>(80 * second->dpiScale()) &&
                hints.min_height == static_cast<int>(40 * second->dpiScale()),
                "X11 native minimum constraints did not follow content zoom");
            XCloseDisplay(display);
        }
    }
#endif
    const auto memory = second->rendererMemoryInfo();
    check(memory.cpuCachesAvailable, "CPU memory diagnostics missing");
    check(memory.gpuCacheAvailable == !software, "GPU cache availability mismatch");
    // Force an actual native resize, where supported, and repaint a fresh back buffer.
    oneui::WindowPlacement placement;
    if (second->getWindowPlacement(placement)) {
        const auto monitors = oneui::enumerateMonitors();
        if (!monitors.empty()) {
            placement.x = static_cast<int>(monitors.front().workArea.x);
            placement.y = static_cast<int>(monitors.front().workArea.y);
        }
        placement.width += 31; placement.height += 17;
        check(second->setWindowPlacement(placement), "Native resize rejected");
    }
    before = second->rendererInfo();
    second->requestRedraw(); waitForPaint(before.paints);
    second->close();
    check(!second->post([] {}), "Closed renderer still accepts work");
    return failures ? 1 : 0;
}

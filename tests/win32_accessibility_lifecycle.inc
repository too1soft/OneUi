// Exercise the real HWND/COM boundary, including a query reentered by teardown.
#include "oneui/platform/window.h"

namespace {
struct AccessibilityCloseProbe {
    WNDPROC previous{};
    IAccessible* accessible{};
    bool observed = false;
    bool disconnected = false;
};

LRESULT CALLBACK accessibilityCloseWindowProc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    auto* probe = static_cast<AccessibilityCloseProbe*>(GetPropW(window, L"OneUIAccessibilityCloseProbe"));
    if (message == WM_DESTROY) {
        LONG children = -1;
        probe->observed = true;
        probe->disconnected = probe->accessible->get_accChildCount(&children) == CO_E_OBJNOTCONNECTED
            && children == 0;
    }
    return CallWindowProcW(probe->previous, window, message, wp, lp);
}

bool accessibilityStopsBeforeWindowDestruction() {
    oneui::WindowOptions options;
    options.visible = false;
    auto window = oneui::Window::create(options);
    auto root = std::make_shared<oneui::View>();
    root->add(std::make_shared<oneui::Button>(L"Accessible action"));
    window->setContent(root);
    window->initialize();
    const auto native = static_cast<HWND>(window->nativeHandle());
    AccessibilityCloseProbe probe;
    if (FAILED(AccessibleObjectFromWindow(native, OBJID_CLIENT, IID_IAccessible,
            reinterpret_cast<void**>(&probe.accessible))) || !probe.accessible) {
        window->close();
        return false;
    }
    LONG children = -1;
    const bool live = SUCCEEDED(probe.accessible->get_accChildCount(&children));
    SetPropW(native, L"OneUIAccessibilityCloseProbe", &probe);
    probe.previous = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(native, GWLP_WNDPROC,
        reinterpret_cast<LONG_PTR>(&accessibilityCloseWindowProc)));
    window->close();
    probe.accessible->Release();
    return live && probe.observed && probe.disconnected && !IsWindow(native);
}
} // namespace

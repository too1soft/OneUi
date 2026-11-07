// Exercise the real HWND/COM boundary, including a query reentered by teardown.
#include "oneui/platform/window.h"
#include <thread>

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
    root->setAccessibleDescription(L"OneUI lifecycle provider");
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
    VARIANT self{}; self.vt = VT_I4; self.lVal = CHILDID_SELF;
    BSTR description = nullptr;
    const bool custom = SUCCEEDED(probe.accessible->get_accDescription(self, &description))
        && description && std::wstring(description) == L"OneUI lifecycle provider";
    SysFreeString(description);
    const bool live = custom && SUCCEEDED(probe.accessible->get_accChildCount(&children));
    SetPropW(native, L"OneUIAccessibilityCloseProbe", &probe);
    probe.previous = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(native, GWLP_WNDPROC,
        reinterpret_cast<LONG_PTR>(&accessibilityCloseWindowProc)));
    window->close();
    probe.accessible->Release();
    return live && probe.observed && probe.disconnected && !IsWindow(native);
}

// MSAA's AccessibleObjectFromWindow supplies its own proxy wrapper. Verify
// standard COM disconnection against the server we actually own, rather than
// incorrectly requiring us to disconnect the OS wrapper's identity as well.
bool pendingAccessibilityCallsAreDisconnected(HWND window, IAccessible* owned) {
    IStream* pending = nullptr;
    const bool marshaled = SUCCEEDED(CoMarshalInterThreadInterfaceInStream(
        IID_IAccessible, owned, &pending));
    oneui::win32_accessibility::retireProvider(window, owned);
    HRESULT remoteCall = E_UNEXPECTED;
    bool initializedClient = false;
    std::atomic_bool finished{false};
    std::thread client([&] {
        const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        initializedClient = SUCCEEDED(initialized);
        IAccessible* stale = nullptr;
        remoteCall = pending ? CoGetInterfaceAndReleaseStream(pending,
            IID_IAccessible, reinterpret_cast<void**>(&stale)) : E_FAIL;
        if (SUCCEEDED(remoteCall) && stale) {
            VARIANT selection{};
            // Unlike IDispatch type-info metadata, this is a real provider
            // method, implemented as unconditional S_FALSE while connected.
            remoteCall = stale->get_accSelection(&selection);
            VariantClear(&selection);
        }
        if (stale) stale->Release();
        if (SUCCEEDED(initialized)) CoUninitialize();
        finished.store(true);
    });
    HANDLE clientThread = client.native_handle();
    while (!finished.load()) {
        DWORD index = 0;
        CoWaitForMultipleHandles(COWAIT_DISPATCH_CALLS | COWAIT_DISPATCH_WINDOW_MESSAGES,
            1000, 1, &clientThread, &index);
    }
    client.join();
    const bool disconnected = marshaled && initializedClient
        && (remoteCall == CO_E_OBJNOTCONNECTED || remoteCall == RPC_E_DISCONNECTED);
    if (!disconnected) std::cerr << "Remote accessibility call survived close: " << std::hex << remoteCall << std::dec << '\n';
    return disconnected;
}
} // namespace

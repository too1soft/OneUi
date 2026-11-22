#include <future>
#include <wrl/client.h>

namespace {

using Microsoft::WRL::ComPtr;

ComPtr<IUIAutomationElement> findAutomationChild(IUIAutomation* automation,
                                                IUIAutomationElement* root, const wchar_t* name) {
    VARIANT property{};
    property.vt = VT_BSTR;
    property.bstrVal = SysAllocString(name);
    ComPtr<IUIAutomationCondition> condition;
    const auto status = automation->CreatePropertyCondition(UIA_NamePropertyId, property, condition.GetAddressOf());
    VariantClear(&property);
    ComPtr<IUIAutomationElement> result;
    if (SUCCEEDED(status)) root->FindFirst(TreeScope_Children, condition.Get(), result.GetAddressOf());
    return result;
}

template<class Interface>
ComPtr<Interface> currentAutomationPattern(IUIAutomationElement* element, PATTERNID id) {
    ComPtr<Interface> result;
    if (element) element->GetCurrentPatternAs(id, IID_PPV_ARGS(result.GetAddressOf()));
    return result;
}

template<class Check>
void testNativeAutomationFragments(Check& check) {
    using namespace oneui;
    using namespace std::chrono_literals;
    const auto module = LoadLibraryExW(L"UIAutomationCore.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!module) return;
    const bool supportsFragments = GetProcAddress(module, "UiaDisconnectProvider") &&
        GetProcAddress(module, "UiaHostProviderFromHwnd");
    const bool supportsLegacyPatterns = GetProcAddress(module, "UiaProviderFromIAccessible") != nullptr;
    FreeLibrary(module);
    if (!supportsFragments) return;
    WindowOptions options;
    options.visible = false;
    auto window = Window::create(options);
    auto root = std::make_shared<OverlayHost>();
    auto background = std::make_shared<View>();
    auto original = std::make_shared<Button>(L"Removed action");
    auto stable = std::make_shared<Button>(L"Retained action");
    auto privateField = std::make_shared<TextField>(L"Protected input");
    background->setAccessibleName(L"Named container");
    privateField->setPasswordMode(true);
    privateField->setText(L"private secret");
    std::atomic<int> activations{0};
    stable->setOnClick([&] { ++activations; });
    background->add(original);
    background->add(stable);
    background->add(privateField);
    root->setContent(background);
    window->setContent(root);
    window->initialize();
    root->setFrame({0, 0, 380, 260});
    background->setFrame(root->frame());
    original->setFrame({10, 20, 140, 28});
    stable->setFrame({10, 60, 140, 28});
    privateField->setFrame({10, 100, 140, 28});
    const auto hwnd = static_cast<HWND>(window->nativeHandle());
    const auto mutate = [&window](std::function<void()> change) {
        auto finished = std::make_shared<std::promise<void>>();
        auto done = finished->get_future();
        if (!window->post([finished, change = std::move(change)] {
                change();
                finished->set_value();
            })) return false;
        return done.wait_for(4s) == std::future_status::ready;
    };
    std::thread client([&] {
        const auto status = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(status)) {
            check(false, "UIA client has a separate apartment");
            return;
        }
        {
            ComPtr<IUIAutomation> automation;
            ComPtr<IUIAutomationElement> clientRoot;
            const auto created = CoCreateInstance(__uuidof(CUIAutomation), nullptr, CLSCTX_INPROC_SERVER,
                                                   IID_PPV_ARGS(automation.GetAddressOf()));
            if (SUCCEEDED(created)) automation->ElementFromHandle(hwnd, clientRoot.GetAddressOf());
            check(clientRoot.Get() != nullptr, "UIA discovers OneUI on the real HWND");
            if (clientRoot) {
                const auto get = [&](const wchar_t* name) {
                    return findAutomationChild(automation.Get(), clientRoot.Get(), name);
                };
                auto removed = get(L"Removed action");
                auto retained = get(L"Retained action");
                auto password = get(L"Protected input");
                check(removed && retained && password, "UIA sees the visible OneUI children");
                auto container = get(L"Named container");
                CONTROLTYPEID controlType = 0;
                check(container && SUCCEEDED(container->get_CurrentControlType(&controlType)) &&
                    controlType == UIA_CustomControlTypeId, "Named generic containers retain the prior UIA control type");
                auto invoke = currentAutomationPattern<IUIAutomationInvokePattern>(retained.Get(), UIA_InvokePatternId);
                auto removedInvoke = currentAutomationPattern<IUIAutomationInvokePattern>(removed.Get(), UIA_InvokePatternId);
                auto legacy = currentAutomationPattern<IUIAutomationLegacyIAccessiblePattern>(retained.Get(), UIA_LegacyIAccessiblePatternId);
                if (supportsLegacyPatterns)
                    check(legacy.Get() != nullptr, "UIA legacy pattern remains available through the system adapter");
                check(invoke && invoke->Invoke() == S_OK && activations.load() == 1,
                    "Native UIA invocation reaches OneUI's real click callback");
                BOOL hasFocus = FALSE;
                check(retained && SUCCEEDED(retained->get_CurrentHasKeyboardFocus(&hasFocus)) && hasFocus,
                    "The focused OneUI child exposes current UIA keyboard focus");
                BOOL protectedState = FALSE;
                check(password && SUCCEEDED(password->get_CurrentIsPassword(&protectedState)) && protectedState,
                    "UIA exposes password state");
                auto value = currentAutomationPattern<IUIAutomationValuePattern>(password.Get(), UIA_ValuePatternId);
                BSTR text = nullptr;
                if (value) value->get_CurrentValue(&text);
                check(!text || std::wstring(text) != L"private secret", "UIA does not expose private text");
                SysFreeString(text);

                check(mutate([original] { original->setVisible(false); }), "Win32 thread hides the original element");
                auto shifted = get(L"Retained action");
                BOOL same = FALSE;
                check(shifted && retained && SUCCEEDED(automation->CompareElements(retained.Get(), shifted.Get(), &same)) && same,
                    "A retained fragment keeps its runtime ID when siblings move");
                check(!get(L"Removed action"), "A hidden element leaves the UIA tree");
                check(removedInvoke && FAILED(removedInvoke->Invoke()) && activations.load() == 1,
                    "A stale fragment cannot invoke the control at its former position");
                text = nullptr;
                if (supportsLegacyPatterns)
                    check(legacy && SUCCEEDED(legacy->get_CurrentName(&text)) && text && std::wstring(text) == L"Retained action",
                        "A cached legacy pattern follows the stable fragment identity");
                SysFreeString(text);

                auto dialog = std::make_shared<Button>(L"Modal fragment");
                check(mutate([root, dialog] {
                    dialog->setFrame({30, 30, 150, 40});
                    root->addOverlay(dialog, OverlayOptions::modal(20));
                }), "Win32 thread opens a modal overlay");
                check(get(L"Modal fragment") && !get(L"Retained action"), "UIA navigation respects modal coverage");
                check(mutate([root, dialog] { root->removeOverlay(dialog.get()); }), "Win32 thread closes the modal overlay");
                check(get(L"Retained action") && !get(L"Modal fragment"), "Closing a modal restores the accessible surface");
            }
        }
        CoUninitialize();
    });
    HANDLE handle = client.native_handle();
    while (WaitForSingleObject(handle, 0) == WAIT_TIMEOUT) {
        DWORD index = 0;
        CoWaitForMultipleHandles(COWAIT_DISPATCH_CALLS | COWAIT_DISPATCH_WINDOW_MESSAGES, 150, 1, &handle, &index);
        MSG message{};
        while (PeekMessageW(&message, hwnd, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    client.join();
    window->close();
}

} // namespace

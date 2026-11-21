#pragma once

#include "platform/win32/compat_win32.h"

#include <oleacc.h>
#include <UIAutomation.h>
#include <utility>

namespace oneui::win32_accessibility {

class AutomationProvider {
public:
    AutomationProvider() = default;
    AutomationProvider(const AutomationProvider&) = delete;
    AutomationProvider& operator=(const AutomationProvider&) = delete;

    ~AutomationProvider() {
        disconnect();
        if (module_) FreeLibrary(module_);
    }

    LRESULT reply(HWND window, WPARAM wp, LPARAM lp, IAccessible* accessible) {
        if (!initialized_) {
            initialized_ = true;
            module_ = win32::loadSystemLibrary(L"UIAutomationCore.dll");
            if (module_) {
                reply_ = reinterpret_cast<decltype(reply_)>(GetProcAddress(module_, "UiaReturnRawElementProvider"));
                disconnect_ = reinterpret_cast<decltype(disconnect_)>(GetProcAddress(module_, "UiaDisconnectProvider"));
                const auto wrap = reinterpret_cast<decltype(&UiaProviderFromIAccessible)>(
                    GetProcAddress(module_, "UiaProviderFromIAccessible"));
                if (wrap && reply_ && disconnect_) {
                    // Wrap the same accessible controls in process. UI Automation
                    // can discover them without first round-tripping through MSAA.
                    wrap(accessible, CHILDID_SELF, UIA_PFIA_DEFAULT, &provider_);
                }
            }
        }
        return provider_ ? reply_(window, wp, lp, provider_) : 0;
    }

    void disconnect() {
        if (auto* provider = std::exchange(provider_, nullptr)) {
            disconnect_(provider);
            provider->Release();
        }
    }

private:
    HMODULE module_ = nullptr;
    IRawElementProviderSimple* provider_ = nullptr;
    decltype(&UiaReturnRawElementProvider) reply_ = nullptr;
    decltype(&UiaDisconnectProvider) disconnect_ = nullptr;
    bool initialized_ = false;
};

} // namespace oneui::win32_accessibility

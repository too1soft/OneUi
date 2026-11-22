#pragma once

#include <windows.h>
#include <oleacc.h>

struct IRawElementProviderSimple;

namespace oneui::win32_accessibility {

class AutomationProvider {
public:
    AutomationProvider() = default;
    AutomationProvider(const AutomationProvider&) = delete;
    AutomationProvider& operator=(const AutomationProvider&) = delete;
    ~AutomationProvider();

    // The accessible object is OneUI's window provider, shared by MSAA and UIA.
    LRESULT reply(HWND window, WPARAM wp, LPARAM lp, IAccessible* accessible);
    void disconnect();

private:
    IRawElementProviderSimple* root_ = nullptr;
    bool unsupported_ = false;
};

} // namespace oneui::win32_accessibility

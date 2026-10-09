#pragma once
#include "compat_win32.h"
#include <roapi.h>
#include <stdexcept>
#include <string>
#include <windows.h>
#include <winstring.h>
#include <wrl/client.h>

namespace oneui::win32 {
using Microsoft::WRL::ComPtr;
inline void checkNotificationHr(HRESULT result) {
    if (FAILED(result))
        throw std::runtime_error("Windows notification HRESULT " +
                                 std::to_string(static_cast<unsigned long>(result)));
}
// Resolve WinRT at runtime so importing OneUI remains possible on Windows 7.
class NotificationRuntime {
    HMODULE module_ = loadSystemLibrary(L"combase.dll");
    template <class T> T load(const char *name) const {
        return module_ ? reinterpret_cast<T>(GetProcAddress(module_, name)) : nullptr;
    }

  public:
    decltype(&WindowsCreateString) createString =
        load<decltype(createString)>("WindowsCreateString");
    decltype(&WindowsDeleteString) deleteString =
        load<decltype(deleteString)>("WindowsDeleteString");
    decltype(&RoGetActivationFactory) factory = load<decltype(factory)>("RoGetActivationFactory");
    decltype(&RoActivateInstance) activate = load<decltype(activate)>("RoActivateInstance");
    bool available() const {
        using VersionQuery = LONG(WINAPI*)(OSVERSIONINFOW*);
        auto query = reinterpret_cast<VersionQuery>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
        OSVERSIONINFOW version{}; version.dwOSVersionInfoSize = sizeof(version);
        return query && query(&version) == 0 && version.dwMajorVersion >= 10 &&
            createString && deleteString && factory && activate;
    }
    // Process lifetime: HSTRING and factory destructors can run during shutdown.
    static NotificationRuntime &get() {
        static NotificationRuntime value;
        return value;
    }
};
class NotificationString {
    HSTRING value_ = nullptr;

  public:
    explicit NotificationString(const std::wstring &value) {
        checkNotificationHr(NotificationRuntime::get().createString(
            value.data(), static_cast<UINT32>(value.size()), &value_));
    }
    ~NotificationString() { NotificationRuntime::get().deleteString(value_); }
    NotificationString(const NotificationString &) = delete;
    operator HSTRING() const { return value_; }
};
template <class T> ComPtr<T> notificationFactory(const wchar_t *name) {
    NotificationString value(name);
    ComPtr<T> result;
    checkNotificationHr(NotificationRuntime::get().factory(value, IID_PPV_ARGS(&result)));
    return result;
}
GUID notificationActivatorId(const std::string &applicationId);
std::wstring notificationGuidString(const GUID &);
std::wstring notificationTag(const std::string &id);
bool notificationRegistrationExists(const std::string &applicationId);
} // namespace oneui::win32

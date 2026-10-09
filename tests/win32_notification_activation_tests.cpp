#include "oneui/application.h"
#include <windows.h>
#include "platform/win32/notification_callback_abi.h"
#include <shlobj.h>
#include <propkey.h>
#include <wrl/client.h>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

namespace {
using namespace oneui;
using Microsoft::WRL::ComPtr;
ApplicationOptions identity() { return {"org.oneui.tests.notification-activation", L"OneUI notification activation test", QuitPolicy::Explicit}; }
void require(bool result, const char* message) { if (!result) throw std::runtime_error(message); }
std::wstring executable() { wchar_t path[32768]{}; require(GetModuleFileNameW(nullptr, path, 32768) > 0, "Executable path unavailable"); return path; }
std::string hex(const std::wstring& value) {
    std::string result; const char digits[] = "0123456789abcdef";
    for (auto c : value) { require(c < 128, "Test event name must be ASCII"); result += digits[c >> 4]; result += digits[c & 15]; } return result;
}
int server() {
    Application app(identity());
    std::mutex mutex; std::condition_variable finished; bool stop = false; bool activated = false;
    auto dispatch = app.dispatcher();
    std::thread watchdog([&] {
        std::unique_lock<std::mutex> lock(mutex);
        if (!finished.wait_for(lock, std::chrono::seconds(20), [&] { return stop; })) dispatch([&] { app.requestQuit(); });
    });
    app.setOnActivation([&](const ApplicationActivation& value) {
        if (value.notificationId != "one" || value.actionId != "open" || value.parameter.rfind("Local\\OneUI.Notification.Test.", 0) != 0) return;
        std::wstring name(value.parameter.begin(), value.parameter.end());
        HANDLE event = OpenEventW(EVENT_MODIFY_STATE, FALSE, name.c_str());
        if (event) { activated = SetEvent(event) != FALSE; CloseHandle(event); }
        app.requestQuit();
    });
    app.run();
    { std::lock_guard<std::mutex> lock(mutex); stop = true; } finished.notify_all(); watchdog.join();
    return activated ? 0 : 1;
}
GUID registeredClsid() {
    PWSTR programs = nullptr; require(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Programs, 0, nullptr, &programs)), "Programs directory unavailable");
    std::wstring path = programs; CoTaskMemFree(programs); path += L"\\org.oneui.tests.notification-activation.lnk";
    ComPtr<IShellLinkW> link; require(SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link))), "Shortcut reader failed");
    ComPtr<IPersistFile> file; require(SUCCEEDED(link.As(&file)) && SUCCEEDED(file->Load(path.c_str(), STGM_READ)), "Installed shortcut missing");
    ComPtr<IPropertyStore> properties; require(SUCCEEDED(link.As(&properties)), "Shortcut properties unavailable");
    PROPVARIANT value{}; require(SUCCEEDED(properties->GetValue(PKEY_AppUserModel_ToastActivatorCLSID, &value)), "Activation CLSID missing");
    require(value.vt == VT_CLSID && value.puuid, "Invalid activation CLSID");
    auto result = *value.puuid; PropVariantClear(&value); return result;
}
void client() {
    require(SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)), "COM initialization failed");
    struct Cleanup { ~Cleanup() { Application::uninstallDesktopIntegration(identity()); CoUninitialize(); } } cleanup;
    auto installed = Application::installDesktopIntegration(identity(), executable());
    require(installed.status == NotificationStatus::Accepted, installed.detail.c_str());
    // An upgrade reuses the exact per-user registration and shortcut.
    require(Application::installDesktopIntegration(identity(), executable()).status == NotificationStatus::Accepted, "Registration upgrade failed");
    auto clsid = registeredClsid();
    GUID nonce{}; require(SUCCEEDED(CoCreateGuid(&nonce)), "Nonce creation failed"); wchar_t guid[40]{}; StringFromGUID2(nonce, guid, 40);
    const auto eventName = std::wstring(L"Local\\OneUI.Notification.Test.") + guid;
    HANDLE event = CreateEventW(nullptr, TRUE, FALSE, eventName.c_str()); require(event != nullptr, "Event creation failed");
    struct Event { HANDLE value; ~Event() { CloseHandle(value); } } eventOwner{event};
    ComPtr<INotificationActivationCallback> activator;
    // No Application exists in this process. COM must cold-launch the registered EXE.
    const auto launched = CoCreateInstance(clsid, nullptr, CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&activator));
    require(SUCCEEDED(launched), "COM cold launch failed");
    const auto bytes = "oneui1:6f6e65:6f70656e:" + hex(eventName);
    const std::wstring arguments(bytes.begin(), bytes.end());
    require(SUCCEEDED(activator->Activate(L"org.oneui.tests.notification-activation", arguments.c_str(), nullptr, 0)), "COM Activate failed");
    require(WaitForSingleObject(event, 10000) == WAIT_OBJECT_0, "Activation did not reach the new application's UI handler");
    activator.Reset();
    require(Application::uninstallDesktopIntegration(identity()).status == NotificationStatus::Accepted, "Uninstall failed");
    require(Application::uninstallDesktopIntegration(identity()).status == NotificationStatus::Accepted, "Repeated uninstall failed");
}
}
int main(int argc, char** argv) {
    try {
        if (argc > 1 && std::string(argv[1]) == "--oneui-notification-server") return server();
        client(); std::cout << "COM cold activation, upgrade and uninstall passed (notification click not exercised)\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}

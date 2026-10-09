#include "internal/unicode.h"
#include "notification_runtime_win32.h"
#include "platform/shared/notification_backend.h"
#include <filesystem>
#include <propidl.h>
#include <propkey.h>
#include <shlobj.h>

namespace oneui::platform {
namespace {
using namespace win32;
struct ComScope {
    HRESULT value = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ~ComScope() {
        if (SUCCEEDED(value))
            CoUninitialize();
    }
};
struct RegistryKey {
    HKEY value = nullptr;
    ~RegistryKey() {
        if (value)
            RegCloseKey(value);
    }
};
std::wstring registrationPath(const ApplicationOptions &options) {
    return L"Software\\Classes\\CLSID\\" +
           notificationGuidString(notificationActivatorId(options.id));
}
void writeValue(HKEY key, const wchar_t *name, const std::wstring &value) {
    checkNotificationHr(HRESULT_FROM_WIN32(
        RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE *>(value.c_str()),
                       static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)))));
}
bool keyOwned(const std::wstring &path, const std::string &id) {
    RegistryKey key;
    const auto opened =
        RegOpenKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, KEY_QUERY_VALUE, &key.value);
    if (opened == ERROR_FILE_NOT_FOUND)
        return true;
    if (opened != ERROR_SUCCESS)
        return false;
    wchar_t owner[256]{};
    DWORD bytes = sizeof(owner), type = 0;
    return RegQueryValueExW(key.value, L"OneUIApplicationId", nullptr, &type,
                            reinterpret_cast<BYTE *>(owner), &bytes) == ERROR_SUCCESS &&
           type == REG_SZ && bytes >= sizeof(wchar_t) && bytes <= sizeof(owner) &&
           owner[bytes / sizeof(wchar_t) - 1] == 0 && unicode::toUtf8(owner) == id;
}
std::filesystem::path shortcutPath(const ApplicationOptions &options) {
    PWSTR path = nullptr;
    checkNotificationHr(SHGetKnownFolderPath(FOLDERID_Programs, KF_FLAG_CREATE, nullptr, &path));
    std::filesystem::path result(path);
    CoTaskMemFree(path);
    return result / (unicode::fromUtf8(options.id) + L".lnk");
}
bool shortcutOwned(const std::filesystem::path &path, const std::string &id) {
    if (!std::filesystem::exists(path))
        return true;
    if (std::filesystem::is_symlink(path))
        return false;
    ComPtr<IShellLinkW> link;
    checkNotificationHr(
        CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link)));
    ComPtr<IPersistFile> persist;
    checkNotificationHr(link.As(&persist));
    checkNotificationHr(persist->Load(path.c_str(), STGM_READ));
    ComPtr<IPropertyStore> properties;
    checkNotificationHr(link.As(&properties));
    PROPVARIANT value{};
    const auto status = properties->GetValue(PKEY_AppUserModel_ID, &value);
    const bool owned = SUCCEEDED(status) && value.vt == VT_LPWSTR && value.pwszVal &&
                       unicode::toUtf8(value.pwszVal) == id;
    PropVariantClear(&value);
    return owned;
}
void saveShortcut(const std::filesystem::path &path, const std::filesystem::path &binary,
                  const ApplicationOptions &options) {
    ComPtr<IShellLinkW> link;
    checkNotificationHr(
        CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link)));
    checkNotificationHr(link->SetPath(binary.c_str()));
    checkNotificationHr(link->SetWorkingDirectory(binary.parent_path().c_str()));
    checkNotificationHr(link->SetDescription(options.name.c_str()));
    ComPtr<IPropertyStore> properties;
    checkNotificationHr(link.As(&properties));
    auto id = unicode::fromUtf8(options.id);
    auto clsid = notificationActivatorId(options.id);
    PROPVARIANT value{};
    value.vt = VT_LPWSTR;
    value.pwszVal = id.data();
    checkNotificationHr(properties->SetValue(PKEY_AppUserModel_ID, value));
    value = {};
    value.vt = VT_CLSID;
    value.puuid = &clsid;
    checkNotificationHr(properties->SetValue(PKEY_AppUserModel_ToastActivatorCLSID, value));
    checkNotificationHr(properties->Commit());
    ComPtr<IPersistFile> persist;
    checkNotificationHr(link.As(&persist));
    checkNotificationHr(persist->Save(path.c_str(), TRUE));
}
} // namespace
NotificationResult registerDesktopApplication(const ApplicationOptions &options,
                                              const std::wstring &executable) {
    try {
        ComScope com;
        if (FAILED(com.value) && com.value != RPC_E_CHANGED_MODE)
            checkNotificationHr(com.value);
        const auto binary = std::filesystem::path(executable).lexically_normal();
        if (!binary.is_absolute() || !std::filesystem::is_regular_file(binary) ||
            executable.find(L'"') != std::wstring::npos)
            return {NotificationStatus::Failed, {}, "Executable must be an existing absolute path"};
        const auto keyPath = registrationPath(options);
        const auto shortcut = shortcutPath(options);
        if (!keyOwned(keyPath, options.id) || !shortcutOwned(shortcut, options.id))
            return {NotificationStatus::Failed,
                    {},
                    "Existing activation registration belongs to another installer"};
        saveShortcut(shortcut, binary, options);
        RegistryKey key, server;
        checkNotificationHr(
            HRESULT_FROM_WIN32(RegCreateKeyExW(HKEY_CURRENT_USER, keyPath.c_str(), 0, nullptr, 0,
                                               KEY_WRITE, nullptr, &key.value, nullptr)));
        writeValue(key.value, L"OneUIApplicationId", unicode::fromUtf8(options.id));
        checkNotificationHr(
            HRESULT_FROM_WIN32(RegCreateKeyExW(key.value, L"LocalServer32", 0, nullptr, 0,
                                               KEY_WRITE, nullptr, &server.value, nullptr)));
        writeValue(server.value, nullptr,
                   L"\"" + binary.wstring() + L"\" --oneui-notification-server");
        writeValue(server.value, L"OneUIExecutable", binary.wstring());
        return {NotificationStatus::Accepted,
                {},
                "Start Menu shortcut and per-user COM activation registered"};
    } catch (const std::exception &error) {
        return {NotificationStatus::Failed, {}, error.what()};
    }
}
NotificationResult unregisterDesktopApplication(const ApplicationOptions &options) {
    try {
        ComScope com;
        const auto path = registrationPath(options);
        const auto shortcut = shortcutPath(options);
        if (!keyOwned(path, options.id) || !shortcutOwned(shortcut, options.id))
            return {NotificationStatus::Failed,
                    {},
                    "Refusing to remove registration owned by another installer"};
        std::filesystem::remove(shortcut);
        const auto result = RegDeleteTreeW(HKEY_CURRENT_USER, path.c_str());
        if (result != ERROR_FILE_NOT_FOUND)
            checkNotificationHr(HRESULT_FROM_WIN32(result));
        return {NotificationStatus::Accepted,
                {},
                "Per-user Start Menu and COM activation registration removed"};
    } catch (const std::exception &error) {
        return {NotificationStatus::Failed, {}, error.what()};
    }
}
} // namespace oneui::platform

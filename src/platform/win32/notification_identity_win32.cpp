#include "internal/unicode.h"
#include "notification_runtime_win32.h"
#include <array>
#include <cstring>

namespace oneui::win32 {
namespace {
std::uint64_t stableHash(const std::string &text, std::uint64_t seed) {
    for (unsigned char c : text) {
        seed ^= c;
        seed *= 1099511628211ull;
    }
    return seed;
}
} // namespace
GUID notificationActivatorId(const std::string &applicationId) {
    // Application-scoped deterministic CLSID; independent of process/address space.
    const std::array<std::uint64_t, 2> bytes{
        {stableHash("oneui.notification:" + applicationId, 14695981039346656037ull),
         stableHash(applicationId + ":notification.oneui", 7809847782465536322ull)}};
    GUID result{};
    std::memcpy(&result, bytes.data(), sizeof(result));
    result.Data3 = (result.Data3 & 0x0fff) | 0x8000;
    result.Data4[0] = (result.Data4[0] & 0x3f) | 0x80;
    return result;
}
std::wstring notificationGuidString(const GUID &value) {
    wchar_t text[40]{};
    StringFromGUID2(value, text, 40);
    return text;
}
std::wstring notificationTag(const std::string &id) {
    auto hash = stableHash(id, 14695981039346656037ull);
    std::wstring tag(16, L'0');
    const wchar_t digits[] = L"0123456789abcdef";
    for (int i = 15; i >= 0; --i) {
        tag[i] = digits[hash & 15];
        hash >>= 4;
    }
    return tag;
}
bool notificationRegistrationExists(const std::string &id) {
    HKEY key = nullptr;
    const auto path = L"Software\\Classes\\CLSID\\" +
                      notificationGuidString(notificationActivatorId(id)) + L"\\LocalServer32";
    if (RegOpenKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
        return false;
    wchar_t executable[32768]{};
    DWORD bytes = sizeof(executable), type = 0;
    const auto result = RegQueryValueExW(key, L"OneUIExecutable", nullptr, &type,
                                         reinterpret_cast<BYTE *>(executable), &bytes);
    RegCloseKey(key);
    wchar_t current[32768]{};
    auto size = GetModuleFileNameW(nullptr, current, 32768);
    return result == ERROR_SUCCESS && type == REG_SZ && bytes >= sizeof(wchar_t) &&
           bytes <= sizeof(executable) && executable[(bytes / sizeof(wchar_t)) - 1] == 0 && size &&
           size < 32768 && _wcsicmp(current, executable) == 0;
}
} // namespace oneui::win32

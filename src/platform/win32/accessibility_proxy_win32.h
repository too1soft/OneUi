#pragma once

#include <windows.h>
#include <oleacc.h>

namespace oneui::win32_accessibility {
namespace proxy {

constexpr CLSID automationMarshaler{
    0x00020424, 0x0000, 0x0000, {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}
};
constexpr CLSID systemMsaaProxy{
    0x03022430, 0xabc4, 0x11d0, {0xbd, 0xe2, 0x00, 0xaa, 0x00, 0x1a, 0x19, 0x53}
};
constexpr GUID officeTypeLibrary{
    0x2df8d04c, 0x5bfa, 0x101b, {0xbd, 0xe5, 0x00, 0xaa, 0x00, 0x44, 0xde, 0x52}
};

inline bool readGuid(HKEY root, const wchar_t* key, GUID& value) {
    wchar_t text[39]{};
    DWORD bytes = sizeof(text);
    return RegGetValueW(root, key, nullptr, RRF_RT_REG_SZ, nullptr, text, &bytes) == ERROR_SUCCESS
        && SUCCEEDED(CLSIDFromString(text, &value));
}

inline HRESULT selectProxy() {
    CLSID current{};
    if (FAILED(CoGetPSClsid(IID_IAccessible, &current)) || current != automationMarshaler)
        return S_FALSE;

    // Some Office installations map the Windows interface to their own type
    // library under the current user. This runs the Automation marshaler and
    // can repeatedly load that library during WM_GETOBJECT on the UI thread.
    GUID userLibrary{};
    CLSID machineProxy{};
    if (!readGuid(HKEY_CURRENT_USER,
                  L"Software\\Classes\\Interface\\{618736E0-3C3D-11CF-810C-00AA00389B71}\\TypeLib",
                  userLibrary) || userLibrary != officeTypeLibrary ||
        !readGuid(HKEY_LOCAL_MACHINE,
                  L"Software\\Classes\\Interface\\{618736E0-3C3D-11CF-810C-00AA00389B71}\\ProxyStubClsid32",
                  machineProxy) || machineProxy != systemMsaaProxy)
        return S_FALSE;

    // COM applies this mapping inside the current process.
    return CoRegisterPSClsid(IID_IAccessible, machineProxy);
}

} // namespace proxy

inline void selectSystemProxyForOfficeConflict() {
    static const HRESULT result = proxy::selectProxy();
    (void)result;
}

} // namespace oneui::win32_accessibility

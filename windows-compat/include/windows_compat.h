/* Shared runtime identity; C/C++ ABI, no additional runtime DLL. */
#ifndef WINDOWS_COMPAT_H
#define WINDOWS_COMPAT_H
#include <windows.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct wc_identity {
    unsigned long major, minor, build;
    unsigned short service_pack;
    /* 1=x86, 2=x86_64. Unknown/ARM is rejected, not guessed. */
    unsigned int native_arch, process_arch;
    /* 1=legacy, 2=modern. This is an installation default only. */
    unsigned int default_profile;
} wc_identity;
static int wc_detect(wc_identity *out) {
    typedef LONG (WINAPI *version_fn)(OSVERSIONINFOEXW *);
    HMODULE ntdll;
    version_fn get_version;
    OSVERSIONINFOEXW version;
    SYSTEM_INFO system;
    if (!out) return 0;
    ZeroMemory(out, sizeof(*out));
    ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return 0;
    get_version = (version_fn)GetProcAddress(ntdll, "RtlGetVersion");
    if (!get_version) return 0;
    ZeroMemory(&version, sizeof(version));
    version.dwOSVersionInfoSize = sizeof(version);
    if (get_version(&version) != 0) return 0;
    out->major = version.dwMajorVersion;
    out->minor = version.dwMinorVersion;
    out->build = version.dwBuildNumber;
    out->service_pack = version.wServicePackMajor;
    GetNativeSystemInfo(&system);
    if (system.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL) out->native_arch = 1;
    else if (system.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) out->native_arch = 2;
    else return 0;
#if defined(_M_IX86) || defined(__i386__)
    out->process_arch = 1;
#elif defined(_M_X64) || defined(__x86_64__)
    out->process_arch = 2;
#else
    return 0;
#endif
    if (out->major >= 10) out->default_profile = 2;
    else if (out->major == 6 && ((out->minor == 1 && out->build >= 7601 && out->service_pack >= 1) ||
        (out->minor == 2 && out->build >= 9200) || (out->minor == 3 && out->build >= 9600))) out->default_profile = 1;
    else return 0;
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif

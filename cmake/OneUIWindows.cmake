# The /MT runtime is a linked dependency, not controlled by WINVER. In v145,
# libcpmt:xtime.obj unconditionally imports a Windows 8 time function. Do not
# silently publish it as a Windows 7 SDK, even when the PE subsystem says 6.1.
set(ONEUI_WINDOWS_BASELINE "win10" CACHE STRING "Minimum Windows product baseline (win7 or win10)")
set_property(CACHE ONEUI_WINDOWS_BASELINE PROPERTY STRINGS win7 win10)
if(NOT ONEUI_WINDOWS_BASELINE MATCHES "^(win7|win10)$")
    message(FATAL_ERROR "ONEUI_WINDOWS_BASELINE must be win7 or win10")
endif()
if(WIN32)
    if(ONEUI_WINDOWS_BASELINE STREQUAL "win7")
        if(MSVC AND MSVC_VERSION GREATER_EQUAL 1950)
            message(FATAL_ERROR "The selected MSVC v145 runtime requires Windows 8 APIs. Use scripts/build-oneui-msvc-bundled.ps1 -MinimumWindows win7 (pinned v143), or explicitly select ONEUI_WINDOWS_BASELINE=win10. Do not relabel v145 binaries as Win7 compatible.")
        endif()
        set(ONEUI_WINDOWS_WINVER 0x0601)
        set(ONEUI_WINDOWS_NTDDI 0x06010000)
    else()
        set(ONEUI_WINDOWS_WINVER 0x0A00)
        set(ONEUI_WINDOWS_NTDDI 0x0A000000)
    endif()
    # Includes the core/text objects and test executables, not just the DLL.
    add_compile_definitions(WINVER=${ONEUI_WINDOWS_WINVER} _WIN32_WINNT=${ONEUI_WINDOWS_WINVER} NTDDI_VERSION=${ONEUI_WINDOWS_NTDDI})
endif()

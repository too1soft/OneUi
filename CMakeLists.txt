cmake_minimum_required(VERSION 3.16)

# Set the deployment baseline before compiler checks and target creation.
# Setting it in OneUIDesktop.cmake leaves core/text targets on another baseline.
include(cmake/OneUIMacOS.cmake)
project(oneui VERSION 0.1.0 LANGUAGES CXX)

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8)
endif()

include(CTest)
option(ONEUI_BUILD_MOBILE_RUNTIME "Build the experimental mobile runtime" OFF)
option(ONEUI_MOBILE_RUNTIME_ONLY "Build only the mobile foundation (no UI renderer)" OFF)
option(ONEUI_BUILD_ANDROID_LAB "Build experimental Android unified workbench host" OFF)
if(ONEUI_BUILD_MOBILE_RUNTIME OR ONEUI_MOBILE_RUNTIME_ONLY OR ONEUI_BUILD_ANDROID_LAB)
    include(cmake/OneUIMobile.cmake)
endif()
if(ONEUI_BUILD_ANDROID_LAB)
    if(NOT ANDROID OR ONEUI_MOBILE_RUNTIME_ONLY)
        message(FATAL_ERROR "Android workbench requires the NDK and ONEUI_MOBILE_RUNTIME_ONLY=OFF")
    endif()
    include(cmake/OneUIAndroid.cmake)
    return()
endif()
if(ONEUI_MOBILE_RUNTIME_ONLY)
    return()
endif()
if(ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "iOS" OR CMAKE_SYSTEM_NAME STREQUAL "OHOS")
    message(FATAL_ERROR "The full OneUI mobile renderer/host is not available yet. Use ONEUI_MOBILE_RUNTIME_ONLY=ON for the experimental foundation. Desktop backends cannot be used on this target.")
endif()
include(cmake/OneUIWindows.cmake)
include(cmake/OneUISources.cmake)
include(cmake/OneUITests.cmake)
option(ONEUI_BUILD_PLATFORM_WIN32 "Build the Win32 platform renderer and gallery integration" ON)
option(ONEUI_BUILD_PLATFORM_DESKTOP "Build native Linux/macOS backends" ON)
option(ONEUI_BUILD_STATIC "Build the OneUI static library alongside the DLL" ON)
option(ONEUI_ENABLE_TEST_FRAME_CAPTURE "Enable test-only native frame PNG capture" OFF)
option(ONEUI_ENABLE_DEVELOPER_TOOLS "Include developer tools in non-Debug QA builds (never for delivery)" OFF)
# Debug includes diagnostics. Release/RelWithDebInfo/MinSizeRel omit recorders,
# overlays, shortcuts, and activation code unless a QA build opts in explicitly.
add_compile_definitions(ONEUI_DEVELOPER_TOOLS=$<IF:$<OR:$<CONFIG:Debug>,$<BOOL:${ONEUI_ENABLE_DEVELOPER_TOOLS}>>,1,0>)
option(ONEUI_BUILD_VIEWC "Build the native .one template compiler" ON)
if(ONEUI_BUILD_VIEWC)
    include(cmake/OneUIView.cmake)
endif()

include(cmake/OneUISkia.cmake)
include(cmake/OneUIText.cmake)

add_library(oneui_core OBJECT ${ONEUI_CORE_SOURCES})
target_link_libraries(oneui_core PUBLIC oneui_text)
include(cmake/OneUIYoga.cmake)
include(cmake/OneUITheme.cmake)
set_target_properties(oneui_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(oneui_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(oneui_core
    PRIVATE
        ONEUI_BUILDING_DLL
)

include(cmake/OneUIViewRuntime.cmake)

if(BUILD_TESTING)
    oneui_register_core_tests()
endif()

if(WIN32 AND ONEUI_BUILD_PLATFORM_WIN32)
    add_library(oneui_platform_win32 OBJECT ${ONEUI_WIN32_SOURCES})

    target_link_libraries(oneui_platform_win32
        PUBLIC
            oneui_core
            oneui_skia
            user32
            gdi32
            shell32
            dwmapi
            imm32
            dwrite
            opengl32
            ole32
            oleacc
            oleaut32
            uuid
            windowscodecs
            fontsub
            usp10
    )

    target_include_directories(oneui_platform_win32
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    if(MINGW)
        target_link_options(oneui_platform_win32
            INTERFACE
                -static-libgcc
                -static-libstdc++
        )
    endif()

    target_compile_definitions(oneui_platform_win32
        PRIVATE
            UNICODE
            _UNICODE
            WIN32_LEAN_AND_MEAN
            NOMINMAX
            ONEUI_BUILDING_DLL
    )
    if(ONEUI_ENABLE_TEST_FRAME_CAPTURE)
        target_compile_definitions(oneui_platform_win32 PRIVATE ONEUI_ENABLE_TEST_FRAME_CAPTURE=1)
    endif()

    add_library(oneui SHARED
        ${ONEUI_C_API_SOURCES}
        $<TARGET_OBJECTS:oneui_core>
        $<TARGET_OBJECTS:oneui_views>
        $<TARGET_OBJECTS:oneui_platform_win32>
    )

    target_link_libraries(oneui
        PRIVATE
            oneui_core
            oneui_skia
            user32
            gdi32
            shell32
            dwmapi
            imm32
            dwrite
            opengl32
            ole32
            oleacc
            oleaut32
            uuid
            fontsub
            usp10
    )

    target_include_directories(oneui
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    if(MINGW)
        target_link_options(oneui
            PRIVATE
                -static
                -static-libgcc
                -static-libstdc++
                -Wl,--exclude-libs,ALL
        )
        target_link_libraries(oneui
            PRIVATE
                -Wl,-Bstatic
                stdc++
                gcc
                winpthread
                -Wl,-Bdynamic
        )
    endif()

    target_compile_definitions(oneui
        PRIVATE
            UNICODE
            _UNICODE
            WIN32_LEAN_AND_MEAN
            NOMINMAX
            ONEUI_BUILDING_DLL
    )

    set_target_properties(oneui PROPERTIES
        PREFIX ""
        OUTPUT_NAME "oneui"
    )

    if(ONEUI_BUILD_STATIC)
        add_library(oneui_static STATIC
            ${ONEUI_C_API_SOURCES}
            $<TARGET_OBJECTS:oneui_core>
            $<TARGET_OBJECTS:oneui_views>
            $<TARGET_OBJECTS:oneui_platform_win32>
        )

        target_link_libraries(oneui_static
            PUBLIC
                oneui_text
                oneui_skia
                user32
                gdi32
                shell32
                dwmapi
                imm32
                dwrite
                opengl32
                ole32
            oleacc
                oleaut32
                uuid
                windowscodecs
                fontsub
                usp10
        )

        target_include_directories(oneui_static
            PUBLIC
                ${CMAKE_CURRENT_SOURCE_DIR}/include
        )

        target_compile_definitions(oneui_static
            PUBLIC
                ONEUI_STATIC
            PRIVATE
                UNICODE
                _UNICODE
                WIN32_LEAN_AND_MEAN
                NOMINMAX
        )
        if(ONEUI_ENABLE_TEST_FRAME_CAPTURE)
            target_compile_definitions(oneui_static PUBLIC ONEUI_ENABLE_TEST_FRAME_CAPTURE=1)
        endif()

        set_target_properties(oneui_static PROPERTIES
            OUTPUT_NAME "oneui_static"
        )

        add_library(OneUI::oneui_static ALIAS oneui_static)
    endif()

    add_subdirectory(examples/gallery)
    add_subdirectory(examples/remote_component_gallery)

    if(BUILD_TESTING)
        oneui_register_native_tests()
    endif()
elseif(UNIX AND ONEUI_BUILD_PLATFORM_DESKTOP)
    include(cmake/OneUIDesktop.cmake)
elseif(WIN32)
    add_library(oneui SHARED
        $<TARGET_OBJECTS:oneui_core>
    )

    target_link_libraries(oneui
        PRIVATE
            oneui_core
    )

    target_include_directories(oneui
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    if(MINGW)
        target_link_options(oneui
            PRIVATE
                -static
                -static-libgcc
                -static-libstdc++
                -Wl,--exclude-libs,ALL
        )
        target_link_libraries(oneui
            PRIVATE
                -Wl,-Bstatic
                stdc++
                gcc
                winpthread
                -Wl,-Bdynamic
        )
    endif()

    target_compile_definitions(oneui
        PRIVATE
            UNICODE
            _UNICODE
            WIN32_LEAN_AND_MEAN
            NOMINMAX
            ONEUI_BUILDING_DLL
    )

    set_target_properties(oneui PROPERTIES
        PREFIX ""
        OUTPUT_NAME "oneui"
    )
endif()

if(TARGET oneui_static AND ONEUI_ENABLE_YOGA)
    target_link_libraries(oneui_static PUBLIC yogacore)
endif()

if(TARGET oneui AND TARGET oneui_mobile_runtime)
    include(cmake/OneUIMobileWidgets.cmake)
endif()

# Record the complete static closure after optional backends are attached.
if(TARGET oneui_static)
    include(cmake/OneUISDKManifest.cmake)
endif()

if(WIN32 AND TARGET oneui AND TARGET oneui-viewc)
    add_subdirectory(examples/workspace_interactions)
endif()

cmake_minimum_required(VERSION 3.16)

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
include(cmake/OneUISources.cmake)
include(cmake/OneUITests.cmake)
option(ONEUI_BUILD_PLATFORM_WIN32 "Build the Win32 platform renderer and gallery integration" ON)
option(ONEUI_BUILD_STATIC "Build the OneUI static library alongside the DLL" ON)

if(WIN32 AND ONEUI_BUILD_PLATFORM_WIN32)
    include(cmake/OneUISkia.cmake)
endif()

add_library(oneui_core OBJECT ${ONEUI_CORE_SOURCES})

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
            oleaut32
            uuid
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
            WINVER=0x0601
            _WIN32_WINNT=0x0601
            ONEUI_BUILDING_DLL
    )

    add_library(oneui SHARED
        ${ONEUI_C_API_SOURCES}
        $<TARGET_OBJECTS:oneui_core>
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
            WINVER=0x0601
            _WIN32_WINNT=0x0601
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
            $<TARGET_OBJECTS:oneui_platform_win32>
        )

        target_link_libraries(oneui_static
            PUBLIC
                oneui_skia
                user32
                gdi32
                shell32
                dwmapi
                imm32
                dwrite
                opengl32
                ole32
                oleaut32
                uuid
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
                WINVER=0x0601
                _WIN32_WINNT=0x0601
        )

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
            WINVER=0x0601
            _WIN32_WINNT=0x0601
            ONEUI_BUILDING_DLL
    )

    set_target_properties(oneui PROPERTIES
        PREFIX ""
        OUTPUT_NAME "oneui"
    )
endif()

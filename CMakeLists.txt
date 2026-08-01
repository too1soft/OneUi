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
option(ONEUI_BUILD_PLATFORM_WIN32 "Build the Win32 platform renderer and gallery integration" ON)

if(WIN32 AND ONEUI_BUILD_PLATFORM_WIN32)
    include(cmake/OneUISkia.cmake)
endif()

add_library(oneui_core OBJECT
    src/core/animation.cpp
    src/core/app_shell.cpp
    src/core/badge.cpp
    src/core/button.cpp
    src/core/button_bridge.cpp
    src/core/card.cpp
    src/core/checkbox.cpp
    src/core/dialog.cpp
    src/core/clipboard.cpp
    src/core/dock_view.cpp
    src/core/form_field.cpp
    src/core/grid.cpp
    src/core/icon.cpp
    src/core/icon_badge.cpp
    src/core/icon_button.cpp
    src/core/icon_view.cpp
    src/core/label.cpp
    src/core/log_view.cpp
    src/core/material3_tokens.cpp
    src/core/menu.cpp
    src/core/nav_item.cpp
    src/core/progress_bar.cpp
    src/core/product_shell.cpp
    src/core/radio_group.cpp
    src/core/realtime_frame_view.cpp
    src/core/remote_input_region.cpp
    src/core/scroll_view.cpp
    src/core/select.cpp
    src/core/separator.cpp
    src/core/sidebar_nav_bridge.cpp
    src/core/slider.cpp
    src/core/stack.cpp
    src/core/status_strip.cpp
    src/core/split_view.cpp
    src/core/style_adapter.cpp
    src/core/style_sheet.cpp
    src/core/style_transition.cpp
    src/core/switch.cpp
    src/core/tabs.cpp
    src/core/table.cpp
    src/core/text_field.cpp
    src/core/text_input_bridge.cpp
    src/core/terminal_view.cpp
    src/core/tree_view.cpp
    src/core/title_bar_bridge.cpp
    src/core/tile.cpp
    src/core/toast.cpp
    src/core/top_bar.cpp
    src/core/validation_message.cpp
    src/core/window_title_bar.cpp
    src/core/list.cpp
    src/core/overlay_host.cpp
    src/core/panel.cpp
    src/core/popup.cpp
    src/core/view.cpp
    src/core/widget.cpp
    src/core/wrap.cpp
)

target_include_directories(oneui_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(oneui_core
    PRIVATE
        ONEUI_BUILDING_DLL
)

if(BUILD_TESTING)
    add_executable(oneui_control_behavior_tests
        tests/control_behavior_tests.cpp
    )

    target_link_libraries(oneui_control_behavior_tests
        PRIVATE
            oneui_core
    )

    target_compile_definitions(oneui_control_behavior_tests
        PRIVATE
            ONEUI_BUILDING_DLL
    )

    add_test(NAME oneui_control_behavior_tests COMMAND oneui_control_behavior_tests)

    add_executable(oneui_overlay_host_behavior_tests
        tests/overlay_host_behavior_tests.cpp
    )

    target_link_libraries(oneui_overlay_host_behavior_tests
        PRIVATE
            oneui_core
    )

    target_compile_definitions(oneui_overlay_host_behavior_tests
        PRIVATE
            ONEUI_BUILDING_DLL
    )

    add_test(NAME oneui_overlay_host_behavior_tests COMMAND oneui_overlay_host_behavior_tests)

    add_executable(oneui_scroll_view_behavior_tests
        tests/scroll_view_behavior_tests.cpp
    )

    target_link_libraries(oneui_scroll_view_behavior_tests
        PRIVATE
            oneui_core
    )

    target_compile_definitions(oneui_scroll_view_behavior_tests
        PRIVATE
            ONEUI_BUILDING_DLL
    )

    add_test(NAME oneui_scroll_view_behavior_tests COMMAND oneui_scroll_view_behavior_tests)

    add_executable(oneui_stack_behavior_tests
        tests/stack_behavior_tests.cpp
    )

    target_link_libraries(oneui_stack_behavior_tests
        PRIVATE
            oneui_core
    )

    target_compile_definitions(oneui_stack_behavior_tests
        PRIVATE
            ONEUI_BUILDING_DLL
    )

    add_test(NAME oneui_stack_behavior_tests COMMAND oneui_stack_behavior_tests)

    add_executable(oneui_realtime_frame_view_behavior_tests
        tests/realtime_frame_view_behavior_tests.cpp
    )

    target_link_libraries(oneui_realtime_frame_view_behavior_tests
        PRIVATE
            oneui_core
    )

    target_compile_definitions(oneui_realtime_frame_view_behavior_tests
        PRIVATE
            ONEUI_BUILDING_DLL
    )

    add_test(NAME oneui_realtime_frame_view_behavior_tests COMMAND oneui_realtime_frame_view_behavior_tests)

    add_executable(oneui_remote_input_region_behavior_tests
        tests/remote_input_region_behavior_tests.cpp
    )

    target_link_libraries(oneui_remote_input_region_behavior_tests
        PRIVATE
            oneui_core
    )

    target_compile_definitions(oneui_remote_input_region_behavior_tests
        PRIVATE
            ONEUI_BUILDING_DLL
    )

    add_test(NAME oneui_remote_input_region_behavior_tests COMMAND oneui_remote_input_region_behavior_tests)

    add_executable(oneui_terminal_view_behavior_tests
        tests/terminal_view_behavior_tests.cpp
    )

    target_link_libraries(oneui_terminal_view_behavior_tests
        PRIVATE
            oneui_core
    )

    target_compile_definitions(oneui_terminal_view_behavior_tests
        PRIVATE
            ONEUI_BUILDING_DLL
    )

    add_test(NAME oneui_terminal_view_behavior_tests COMMAND oneui_terminal_view_behavior_tests)

    add_executable(oneui_tree_view_behavior_tests
        tests/tree_view_behavior_tests.cpp
    )

    target_link_libraries(oneui_tree_view_behavior_tests
        PRIVATE
            oneui_core
    )

    target_compile_definitions(oneui_tree_view_behavior_tests
        PRIVATE
            ONEUI_BUILDING_DLL
    )

    add_test(NAME oneui_tree_view_behavior_tests COMMAND oneui_tree_view_behavior_tests)

    add_executable(oneui_panel_behavior_tests
        tests/panel_behavior_tests.cpp
    )

    target_link_libraries(oneui_panel_behavior_tests
        PRIVATE
            oneui_core
    )

    target_compile_definitions(oneui_panel_behavior_tests
        PRIVATE
            ONEUI_BUILDING_DLL
    )

    add_test(NAME oneui_panel_behavior_tests COMMAND oneui_panel_behavior_tests)
endif()

if(WIN32 AND ONEUI_BUILD_PLATFORM_WIN32)
    add_library(oneui_platform_win32 OBJECT
        src/platform/win32/monitor_win32.cpp
        src/platform/win32/window_win32.cpp
    )

    target_link_libraries(oneui_platform_win32
        PUBLIC
            oneui_core
            oneui_skia
            user32
            gdi32
            shell32
            dwmapi
            dwrite
            opengl32
            ole32
            oleaut32
            uuid
            fontsub
            usp10
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
        src/capi/oneui_c_api.cpp
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

    add_subdirectory(examples/gallery)
    add_subdirectory(examples/remote_component_gallery)

    if(BUILD_TESTING)
        add_executable(oneui_c_api_behavior_tests
            tests/c_api_behavior_tests.cpp
        )

        target_link_libraries(oneui_c_api_behavior_tests
            PRIVATE
                oneui
        )

        add_test(NAME oneui_c_api_behavior_tests COMMAND oneui_c_api_behavior_tests)

        add_executable(oneui_monitor_behavior_tests
            tests/monitor_behavior_tests.cpp
        )

        target_link_libraries(oneui_monitor_behavior_tests
            PRIVATE
                oneui
        )

        target_compile_definitions(oneui_monitor_behavior_tests
            PRIVATE
                ONEUI_BUILDING_DLL
        )

        add_test(NAME oneui_monitor_behavior_tests COMMAND oneui_monitor_behavior_tests)

        add_executable(oneui_backend_contract_tests
            tests/backend_contract_tests.cpp
        )

        target_link_libraries(oneui_backend_contract_tests
            PRIVATE
                oneui
        )

        target_compile_definitions(oneui_backend_contract_tests
            PRIVATE
                ONEUI_BUILDING_DLL
        )

        add_test(NAME oneui_backend_contract_tests COMMAND oneui_backend_contract_tests)
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

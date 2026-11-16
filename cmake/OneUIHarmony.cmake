# The production producer uses the official OHOS compiler/sysroot. A Linux
# N-API harness exercises the same raster/Widget code without claiming a HAP.
include(${CMAKE_CURRENT_LIST_DIR}/OneUISources.cmake)
if(CMAKE_SYSTEM_NAME STREQUAL "OHOS")
    # Never select the existing Linux/Android Skia archive by platform fallback.
    if(NOT ONEUI_BUNDLED_SKIA_OUT OR NOT EXISTS "${ONEUI_BUNDLED_SKIA_OUT}/verification.json")
        message(FATAL_ERROR "Provide an explicitly verified Skia build from the genuine OHOS compiler/sysroot")
    endif()
    file(READ "${ONEUI_BUNDLED_SKIA_OUT}/verification.json" _oneui_ohos_skia)
    if(NOT _oneui_ohos_skia MATCHES "\"target\"[ \r\n\t]*:[ \r\n\t]*\"HarmonyOS\"" OR
       NOT _oneui_ohos_skia MATCHES "\"status\"[ \r\n\t]*:[ \r\n\t]*\"pass\"" OR
       NOT _oneui_ohos_skia MATCHES "\"genuine_ohos_toolchain\"[ \r\n\t]*:[ \r\n\t]*true")
        message(FATAL_ERROR "The supplied Skia receipt does not attest a genuine HarmonyOS target")
    endif()
endif()
include(${CMAKE_CURRENT_LIST_DIR}/OneUISkia.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/OneUIText.cmake)
add_compile_definitions(ONEUI_DEVELOPER_TOOLS=0)
add_library(oneui_core OBJECT ${ONEUI_CORE_SOURCES})
target_include_directories(oneui_core PUBLIC include PRIVATE src)
target_link_libraries(oneui_core PUBLIC oneui_text)
target_compile_definitions(oneui_core PRIVATE ONEUI_BUILDING_DLL)
set_target_properties(oneui_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
include(${CMAKE_CURRENT_LIST_DIR}/OneUIYoga.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/OneUITheme.cmake)
add_library(oneui_harmony_application SHARED
    src/mobile/widget_host.cpp src/mobile/touch_scroll.cpp src/mobile/text_input.cpp src/mobile/application.cpp
    src/mobile/native_module_posix.cpp src/platform/shared/skia_canvas.cpp
    src/platform/shared/skia_pixel_image.cpp src/platform/shared/skia_path.cpp
    src/platform/harmonyos/application_surface.cpp platform/harmonyos/native/napi_support.cpp
    platform/harmonyos/native/application_module.cpp platform/harmonyos/native/application_napi.cpp
    platform/harmonyos/native/application_input_napi.cpp)
target_include_directories(oneui_harmony_application PUBLIC include PRIVATE src platform/harmonyos/native)
target_link_libraries(oneui_harmony_application PUBLIC oneui_core OneUI::mobile_runtime PRIVATE ${CMAKE_DL_LIBS})
target_compile_definitions(oneui_harmony_application PRIVATE ONEUI_BUILDING_DLL)
target_compile_options(oneui_harmony_application PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter)
target_link_options(oneui_harmony_application PRIVATE -Wl,--no-undefined)
if(ONEUI_HARMONY_HOST_TEST)
    target_include_directories(oneui_harmony_application PRIVATE /usr/include/node)
    target_compile_definitions(oneui_harmony_application PRIVATE ONEUI_NODE_TEST)
    # Resolve N-API against the installed matching host runtime, keeping the
    # same no-undefined check as the production ace_napi link.
    find_library(ONEUI_NODE_RUNTIME_LIBRARY node REQUIRED)
    target_link_libraries(oneui_harmony_application PRIVATE "${ONEUI_NODE_RUNTIME_LIBRARY}")
    set_target_properties(oneui_harmony_application PROPERTIES PREFIX "" OUTPUT_NAME oneui_application SUFFIX ".node")
else()
    target_link_options(oneui_harmony_application PRIVATE -Wl,-z,max-page-size=16384 -Wl,-soname,liboneui_application.so)
    target_link_libraries(oneui_harmony_application PRIVATE ace_napi.z)
    set_target_properties(oneui_harmony_application PROPERTIES OUTPUT_NAME oneui_application)
    include(GNUInstallDirs)
    install(TARGETS oneui_harmony_application LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
    install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
    install(FILES "${CMAKE_CURRENT_LIST_DIR}/OneUIHarmonyConfig.cmake" DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/OneUIHarmony")
endif()

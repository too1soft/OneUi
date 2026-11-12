# Opt-in Android widget renderer. This is not yet the complete desktop Window /
# services API and deliberately does not export a production mobile SDK package.
include(${CMAKE_CURRENT_LIST_DIR}/OneUISources.cmake)
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

add_library(oneui_android_widgets STATIC
    src/mobile/widget_host.cpp src/mobile/touch_scroll.cpp src/mobile/text_input.cpp src/mobile/application.cpp
    src/platform/shared/skia_canvas.cpp src/platform/shared/skia_pixel_image.cpp src/platform/shared/skia_path.cpp
    src/platform/android/skia_surface_android.cpp
    src/platform/android/picture_raster_worker.cpp src/platform/android/egl_picture_backend.cpp)
target_include_directories(oneui_android_widgets PUBLIC include PRIVATE src)
target_link_libraries(oneui_android_widgets PUBLIC oneui_core OneUI::mobile_runtime EGL GLESv3 log)
set_target_properties(oneui_android_widgets PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(ONEUI_BUILD_ANDROID_LAB)
    add_subdirectory(examples/performance_lab/mobile/android)
endif()
if(ONEUI_BUILD_ANDROID_APPLICATION_HOST)
    add_library(oneui_android_application_host STATIC src/platform/android/application_host_android.cpp
        src/platform/android/application_text_android.cpp)
    target_include_directories(oneui_android_application_host PUBLIC include PRIVATE src)
    target_link_libraries(oneui_android_application_host PUBLIC oneui_android_widgets)
    set_target_properties(oneui_android_application_host PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
if(BUILD_TESTING)
    add_executable(oneui_raster_worker_tests tests/mobile/picture_raster_worker_tests.cpp)
    target_include_directories(oneui_raster_worker_tests PRIVATE src)
    target_link_libraries(oneui_raster_worker_tests PRIVATE oneui_android_widgets)
    add_executable(oneui_mobile_text_tests tests/mobile/text_input_tests.cpp tests/mobile/selection_tests.cpp)
    target_link_libraries(oneui_mobile_text_tests PRIVATE oneui_android_widgets)
endif()

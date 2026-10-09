# This optional bridge uses the same controls as the desktop SDK. It is kept
# separate from the small no-renderer runtime so Android NDK contract builds do
# not accidentally require desktop Skia/text libraries.
add_library(oneui_mobile_widgets STATIC src/mobile/widget_host.cpp src/mobile/touch_scroll.cpp)
add_library(OneUI::mobile_widgets ALIAS oneui_mobile_widgets)
target_include_directories(oneui_mobile_widgets PUBLIC include)
target_link_libraries(oneui_mobile_widgets PUBLIC OneUI::mobile_runtime oneui)
if(BUILD_TESTING)
    add_executable(oneui_mobile_widget_tests tests/mobile/widget_host_tests.cpp)
    target_include_directories(oneui_mobile_widget_tests PRIVATE tests)
    target_link_libraries(oneui_mobile_widget_tests PRIVATE OneUI::mobile_widgets)
    add_test(NAME oneui_mobile_widget_tests COMMAND oneui_mobile_widget_tests)
endif()

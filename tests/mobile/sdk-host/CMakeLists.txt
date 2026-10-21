cmake_minimum_required(VERSION 3.20)
project(OneUIMobileWidgetConsumer LANGUAGES CXX)
include(CTest)
if(NOT MSVC OR NOT ONEUI_SDK_DIR)
    message(FATAL_ERROR "This fixture requires MSVC and a prebuilt Windows ONEUI_SDK_DIR")
endif()
get_filename_component(_root "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
file(STRINGS "${ONEUI_SDK_DIR}/static-libraries.txt" _static)
file(STRINGS "${ONEUI_SDK_DIR}/system-libraries.txt" _system)
set(_libraries)
foreach(_name IN LISTS _static)
    set(_library "${ONEUI_SDK_DIR}/lib/${_name}.lib")
    if(NOT EXISTS "${_library}")
        message(FATAL_ERROR "Missing prebuilt SDK library: ${_library}")
    endif()
    list(APPEND _libraries "${_library}")
endforeach()
add_executable(oneui_mobile_widget_tests
    ../widget_host_tests.cpp
    "${_root}/src/mobile/widget_host.cpp"
    "${_root}/src/mobile/touch_scroll.cpp"
    "${_root}/src/mobile/runtime.cpp")
target_compile_features(oneui_mobile_widget_tests PRIVATE cxx_std_17)
target_compile_options(oneui_mobile_widget_tests PRIVATE /utf-8)
target_compile_definitions(oneui_mobile_widget_tests PRIVATE ONEUI_STATIC ONEUI_MOBILE_STATIC)
set_property(TARGET oneui_mobile_widget_tests PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
# SDK headers take precedence. Only the new mobile headers come from the source tree.
target_include_directories(oneui_mobile_widget_tests PRIVATE
    "${ONEUI_SDK_DIR}/include" "${_root}/include" "${_root}/tests")
target_link_libraries(oneui_mobile_widget_tests PRIVATE ${_libraries} ${_system})
add_test(NAME oneui_mobile_widget_tests COMMAND oneui_mobile_widget_tests)

# Compile the exact page used by both hosts against the frozen desktop SDK.
add_executable(oneui_mobile_layout_tests ../layout_page_tests.cpp
    "${_root}/examples/performance_lab/demo_page.cpp"
    "${_root}/examples/performance_lab/pages/basic_layout_page.cpp"
    "${_root}/examples/performance_lab/pages/basic_controls_page.cpp"
    "${_root}/src/mobile/widget_host.cpp" "${_root}/src/mobile/touch_scroll.cpp"
    "${_root}/src/mobile/runtime.cpp")
target_compile_features(oneui_mobile_layout_tests PRIVATE cxx_std_17)
target_compile_options(oneui_mobile_layout_tests PRIVATE /utf-8)
target_compile_definitions(oneui_mobile_layout_tests PRIVATE ONEUI_STATIC ONEUI_MOBILE_STATIC)
set_property(TARGET oneui_mobile_layout_tests PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
target_include_directories(oneui_mobile_layout_tests PRIVATE
    "${ONEUI_SDK_DIR}/include" "${_root}/include" "${_root}/tests"
    "${_root}/examples/performance_lab")
target_link_libraries(oneui_mobile_layout_tests PRIVATE ${_libraries} ${_system})
add_test(NAME oneui_mobile_layout_tests COMMAND oneui_mobile_layout_tests)

add_executable(oneui_mobile_text_tests ../text_input_tests.cpp ../selection_tests.cpp "${_root}/src/mobile/text_input.cpp")
target_compile_features(oneui_mobile_text_tests PRIVATE cxx_std_17)
target_compile_options(oneui_mobile_text_tests PRIVATE /utf-8)
target_compile_definitions(oneui_mobile_text_tests PRIVATE ONEUI_STATIC ONEUI_MOBILE_STATIC)
set_property(TARGET oneui_mobile_text_tests PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
target_include_directories(oneui_mobile_text_tests PRIVATE "${ONEUI_SDK_DIR}/include" "${_root}/include")
target_link_libraries(oneui_mobile_text_tests PRIVATE ${_libraries} ${_system})
add_test(NAME oneui_mobile_text_tests COMMAND oneui_mobile_text_tests)

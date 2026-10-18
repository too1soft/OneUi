add_library(oneui_mobile_lab SHARED host.cpp text_input.cpp
    platform_metrics.cpp ../workbench.cpp ../performance_page.cpp
    ../../demo_catalog.cpp ../../demo_page.cpp ../../pages/basic_layout_page.cpp
    ../../pages/basic_controls_page.cpp ../appearance_page.cpp ../appearance_snippet.cpp ../text_page.cpp ../text_snippet.cpp)

# Embed exactly the checked-in examples that this target also compiles.
set(_snippet_header "#pragma once\nnamespace lab::mobile::snippets {\n")
foreach(_snippet basic_layout controls charts particles)
    set(_path "${CMAKE_CURRENT_SOURCE_DIR}/../../snippets/cpp/${_snippet}.cpp")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_path}")
    file(READ "${_path}" _code)
    string(APPEND _snippet_header "inline constexpr const char *${_snippet} = R\"ONEUI_CODE(${_code})ONEUI_CODE\";\n")
    target_sources(oneui_mobile_lab PRIVATE "${_path}")
endforeach()
string(APPEND _snippet_header "}\n")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../appearance_snippet.cpp" _appearance_code)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../appearance_snippet.cpp")
string(APPEND _snippet_header "namespace lab::mobile::snippets { inline constexpr const char *appearance = R\"ONEUI_CODE(${_appearance_code})ONEUI_CODE\"; }\n")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../text_snippet.cpp" _text_code)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../text_snippet.cpp")
string(APPEND _snippet_header "namespace lab::mobile::snippets { inline constexpr const char *text_input = R\"ONEUI_CODE(${_text_code})ONEUI_CODE\"; }\n")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/mobile_snippets.hpp" "${_snippet_header}")
target_include_directories(oneui_mobile_lab PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/examples/performance_lab")
target_link_libraries(oneui_mobile_lab PRIVATE oneui_android_widgets)
# Keep one copy of the C++ runtime in this experimental APK.
target_link_options(oneui_mobile_lab PRIVATE -Wl,--gc-sections -Wl,-z,max-page-size=16384)

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/Runtime.oneview"
    COMMAND oneui-viewc --emit ir --input "${PROJECT_SOURCE_DIR}/tests/views/Runtime.one"
        --schema "${PROJECT_SOURCE_DIR}/tests/views/Runtime.oneschema" --name Runtime
        --output "${CMAKE_CURRENT_BINARY_DIR}/Runtime.oneview"
    DEPENDS oneui-viewc tests/views/Runtime.one tests/views/Runtime.oneschema VERBATIM)
add_custom_target(oneui_view_test_ir DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/Runtime.oneview")
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/Overlay.oneview"
    COMMAND oneui-viewc --emit ir --input "${PROJECT_SOURCE_DIR}/tests/views/Overlay.one"
        --schema "${PROJECT_SOURCE_DIR}/tests/views/Runtime.oneschema" --name Overlay
        --output "${CMAKE_CURRENT_BINARY_DIR}/Overlay.oneview"
    DEPENDS oneui-viewc tests/views/Overlay.one tests/views/Imported.one tests/views/Runtime.oneschema VERBATIM)
add_custom_target(oneui_view_overlay_ir DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/Overlay.oneview")
add_dependencies(oneui_view_test_ir oneui_view_overlay_ir)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/Advanced.oneview"
    COMMAND oneui-viewc --emit ir --input "${PROJECT_SOURCE_DIR}/tests/views/Advanced.one"
        --schema "${PROJECT_SOURCE_DIR}/tests/views/Runtime.oneschema" --name Advanced
        --output "${CMAKE_CURRENT_BINARY_DIR}/Advanced.oneview"
    DEPENDS oneui-viewc tests/views/Advanced.one tests/views/Slotted.one tests/views/Runtime.oneschema VERBATIM)
add_custom_target(oneui_view_advanced_ir DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/Advanced.oneview")
add_dependencies(oneui_view_test_ir oneui_view_advanced_ir)
add_executable(oneui_view_runtime_tests tests/views/runtime_tests.cpp)
target_link_libraries(oneui_view_runtime_tests PRIVATE oneui_views oneui_core)
target_compile_definitions(oneui_view_runtime_tests PRIVATE ONEUI_BUILDING_DLL)
target_include_directories(oneui_view_runtime_tests PRIVATE src)
add_dependencies(oneui_view_runtime_tests oneui_view_test_ir)
add_test(NAME oneui_view_runtime_tests COMMAND oneui_view_runtime_tests "${CMAKE_CURRENT_BINARY_DIR}/Runtime.oneview")
add_executable(oneui_view_list_tests tests/views/list_tests.cpp)
target_link_libraries(oneui_view_list_tests PRIVATE oneui_views oneui_core)
target_compile_definitions(oneui_view_list_tests PRIVATE ONEUI_BUILDING_DLL)
target_include_directories(oneui_view_list_tests PRIVATE src)
add_dependencies(oneui_view_list_tests oneui_view_test_ir)
add_test(NAME oneui_view_list_tests COMMAND oneui_view_list_tests "${CMAKE_CURRENT_BINARY_DIR}/Runtime.oneview")
set(view_c_smoke "${CMAKE_CURRENT_BINARY_DIR}/oneui_view_c_header_tests${CMAKE_EXECUTABLE_SUFFIX}")
if(MSVC)
    add_custom_command(OUTPUT "${view_c_smoke}"
        COMMAND "${CMAKE_CXX_COMPILER}" /nologo /TC /std:c11 /MD /utf-8
            "/I${PROJECT_SOURCE_DIR}/include" /DONEUI_STATIC
            "${PROJECT_SOURCE_DIR}/tests/views/c_header.c"
            "/Fo${CMAKE_CURRENT_BINARY_DIR}/view_c_header.obj" "/Fe${view_c_smoke}"
        DEPENDS tests/views/c_header.c include/oneui/oneui_view_c_api.h VERBATIM)
else()
    add_custom_command(OUTPUT "${view_c_smoke}"
        COMMAND "${CMAKE_CXX_COMPILER}" -x c -std=c11 -DONEUI_STATIC
            "-I${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/tests/views/c_header.c"
            -o "${view_c_smoke}"
        DEPENDS tests/views/c_header.c include/oneui/oneui_view_c_api.h VERBATIM)
endif()
add_custom_target(oneui_view_c_header_build ALL DEPENDS "${view_c_smoke}")
add_test(NAME oneui_view_c_header_tests COMMAND "${view_c_smoke}")

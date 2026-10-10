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
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/TableRecords.oneview"
    COMMAND oneui-viewc --emit ir --input "${PROJECT_SOURCE_DIR}/tests/views/TableRecords.one"
        --schema "${PROJECT_SOURCE_DIR}/tests/views/TableRecords.oneschema"
        --output "${CMAKE_CURRENT_BINARY_DIR}/TableRecords.oneview"
    DEPENDS oneui-viewc tests/views/TableRecords.one tests/views/TableRecords.oneschema VERBATIM)
add_custom_target(oneui_view_records_ir DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/TableRecords.oneview")
add_dependencies(oneui_view_test_ir oneui_view_records_ir)
add_executable(oneui_view_list_tests tests/views/list_tests.cpp tests/views/table_record_tests.cpp)
target_link_libraries(oneui_view_list_tests PRIVATE oneui_views oneui_core)
target_compile_definitions(oneui_view_list_tests PRIVATE ONEUI_BUILDING_DLL)
target_include_directories(oneui_view_list_tests PRIVATE src)
add_dependencies(oneui_view_list_tests oneui_view_test_ir)
add_test(NAME oneui_view_list_tests COMMAND oneui_view_list_tests "${CMAKE_CURRENT_BINARY_DIR}/Runtime.oneview")
add_executable(oneui_view_reload_tests tests/views/reload_tests.cpp)
target_link_libraries(oneui_view_reload_tests PRIVATE oneui_views oneui_core)
target_compile_definitions(oneui_view_reload_tests PRIVATE ONEUI_BUILDING_DLL)
target_include_directories(oneui_view_reload_tests PRIVATE src)
add_dependencies(oneui_view_reload_tests oneui_view_test_ir)
foreach(fixture Reload ReloadRemoved ReloadEmpty)
    add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${fixture}.oneview"
        COMMAND oneui-viewc --emit ir --input "${PROJECT_SOURCE_DIR}/tests/views/${fixture}.one"
            --schema "${PROJECT_SOURCE_DIR}/tests/views/Runtime.oneschema" --name "${fixture}"
            --output "${CMAKE_CURRENT_BINARY_DIR}/${fixture}.oneview"
        DEPENDS oneui-viewc "tests/views/${fixture}.one" tests/views/Runtime.oneschema VERBATIM)
endforeach()
add_custom_target(oneui_view_reload_ir DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/Reload.oneview" "${CMAKE_CURRENT_BINARY_DIR}/ReloadRemoved.oneview" "${CMAKE_CURRENT_BINARY_DIR}/ReloadEmpty.oneview")
add_dependencies(oneui_view_reload_tests oneui_view_reload_ir)
add_test(NAME oneui_view_reload_tests COMMAND oneui_view_reload_tests "${CMAKE_CURRENT_BINARY_DIR}/Reload.oneview" "${CMAKE_CURRENT_BINARY_DIR}/ReloadRemoved.oneview" "${CMAKE_CURRENT_BINARY_DIR}/ReloadEmpty.oneview")
add_executable(oneui_view_cpp_diff_tests tests/views/cpp_diff_tests.cpp)
target_link_libraries(oneui_view_cpp_diff_tests PRIVATE oneui)
target_include_directories(oneui_view_cpp_diff_tests PRIVATE tests/views tests)
oneui_target_view(oneui_view_cpp_diff_tests NAME DiffIr SOURCE tests/views/Diff.one
    SCHEMA tests/views/Diff.oneschema VM_HEADER diff_vm.h)
add_test(NAME oneui_view_cpp_diff_tests COMMAND oneui_view_cpp_diff_tests)
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

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
add_executable(oneui_pane_motion_tests tests/workspace/pane_motion_tests.cpp)
target_link_libraries(oneui_pane_motion_tests PRIVATE oneui_core)
target_include_directories(oneui_pane_motion_tests PRIVATE src)
target_compile_definitions(oneui_pane_motion_tests PRIVATE ONEUI_BUILDING_DLL)
add_test(NAME oneui_pane_motion_tests COMMAND oneui_pane_motion_tests)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/PaneGrid.oneview"
    COMMAND oneui-viewc --emit ir --input "${PROJECT_SOURCE_DIR}/tests/workspace/PaneGrid.one"
        --schema "${PROJECT_SOURCE_DIR}/tests/workspace/PaneGrid.oneschema"
        --output "${CMAKE_CURRENT_BINARY_DIR}/PaneGrid.oneview"
    DEPENDS oneui-viewc tests/workspace/PaneGrid.one tests/workspace/PaneGrid.oneschema VERBATIM)
add_custom_target(oneui_pane_grid_ir DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/PaneGrid.oneview")
add_executable(oneui_pane_runtime_tests tests/workspace/pane_runtime_tests.cpp)
add_executable(oneui_container_query_tests tests/workspace/container_query_tests.cpp)
target_link_libraries(oneui_container_query_tests PRIVATE oneui_views oneui_core)
target_include_directories(oneui_container_query_tests PRIVATE src)
target_compile_definitions(oneui_container_query_tests PRIVATE ONEUI_BUILDING_DLL)
add_test(NAME oneui_container_query_tests COMMAND oneui_container_query_tests)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/ContainerQuery.oneview"
    COMMAND oneui-viewc --emit ir --input "${PROJECT_SOURCE_DIR}/tests/workspace/ContainerQuery.one"
        --schema "${PROJECT_SOURCE_DIR}/tests/workspace/ContainerQuery.oneschema"
        --output "${CMAKE_CURRENT_BINARY_DIR}/ContainerQuery.oneview"
    DEPENDS oneui-viewc tests/workspace/ContainerQuery.one tests/workspace/ContainerQuery.oneschema VERBATIM)
add_custom_target(oneui_container_query_ir DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/ContainerQuery.oneview")
add_executable(oneui_container_runtime_tests tests/workspace/container_runtime_tests.cpp)
target_link_libraries(oneui_container_runtime_tests PRIVATE oneui_views oneui_core)
target_include_directories(oneui_container_runtime_tests PRIVATE src)
target_compile_definitions(oneui_container_runtime_tests PRIVATE ONEUI_BUILDING_DLL)
add_dependencies(oneui_container_runtime_tests oneui_container_query_ir)
add_test(NAME oneui_container_runtime_tests COMMAND oneui_container_runtime_tests "${CMAKE_CURRENT_BINARY_DIR}/ContainerQuery.oneview")
target_link_libraries(oneui_pane_runtime_tests PRIVATE oneui_views oneui_core)
target_include_directories(oneui_pane_runtime_tests PRIVATE src)
target_compile_definitions(oneui_pane_runtime_tests PRIVATE ONEUI_BUILDING_DLL)
add_dependencies(oneui_pane_runtime_tests oneui_pane_grid_ir)
add_test(NAME oneui_pane_runtime_tests COMMAND oneui_pane_runtime_tests "${CMAKE_CURRENT_BINARY_DIR}/PaneGrid.oneview")
if(WIN32 AND ONEUI_BUILD_STATIC)
    add_executable(oneui_pane_motion_smoke tests/workspace/pane_motion_smoke.cpp)
    target_link_libraries(oneui_pane_motion_smoke PRIVATE oneui_static)
    target_include_directories(oneui_pane_motion_smoke PRIVATE src)
    target_compile_definitions(oneui_pane_motion_smoke PRIVATE ONEUI_STATIC ONEUI_BUILDING_DLL)
endif()
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

add_executable(oneui_workspace_tree_tests tests/workspace/tree_tests.cpp)
add_executable(oneui_pane_interaction_tests tests/workspace/pane_interaction_tests.cpp)
target_link_libraries(oneui_pane_interaction_tests PRIVATE oneui_core)
target_include_directories(oneui_pane_interaction_tests PRIVATE src)
target_compile_definitions(oneui_pane_interaction_tests PRIVATE ONEUI_BUILDING_DLL)
add_test(NAME oneui_pane_interaction_tests COMMAND oneui_pane_interaction_tests)
target_link_libraries(oneui_workspace_tree_tests PRIVATE oneui_core)
target_include_directories(oneui_workspace_tree_tests PRIVATE src)
target_compile_definitions(oneui_workspace_tree_tests PRIVATE ONEUI_BUILDING_DLL)
add_test(NAME oneui_workspace_tree_tests COMMAND oneui_workspace_tree_tests)

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/Workspace.oneview"
    COMMAND oneui-viewc --emit ir --input "${PROJECT_SOURCE_DIR}/tests/views/Workspace.one"
        --schema "${PROJECT_SOURCE_DIR}/tests/views/Runtime.oneschema"
        --output "${CMAKE_CURRENT_BINARY_DIR}/Workspace.oneview"
    DEPENDS oneui-viewc tests/views/Workspace.one tests/views/WorkspaceMirror.one tests/views/Runtime.oneschema VERBATIM)
add_custom_target(oneui_workspace_scope_ir DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/Workspace.oneview")
if(ONEUI_BUILD_STATIC)
    add_executable(oneui_workspace_scope_tests tests/views/workspace_scope_tests.cpp)
    target_link_libraries(oneui_workspace_scope_tests PRIVATE oneui_static)
    target_include_directories(oneui_workspace_scope_tests PRIVATE src)
    target_compile_definitions(oneui_workspace_scope_tests PRIVATE ONEUI_STATIC)
    add_dependencies(oneui_workspace_scope_tests oneui_workspace_scope_ir)
    add_test(NAME oneui_workspace_scope_tests COMMAND oneui_workspace_scope_tests "${CMAKE_CURRENT_BINARY_DIR}/Workspace.oneview")
endif()

add_executable(oneui_dock_workspace_tests tests/workspace/dock_workspace_tests.cpp)
target_link_libraries(oneui_dock_workspace_tests PRIVATE oneui_core)
target_include_directories(oneui_dock_workspace_tests PRIVATE src)
target_compile_definitions(oneui_dock_workspace_tests PRIVATE ONEUI_BUILDING_DLL)
add_test(NAME oneui_dock_workspace_tests COMMAND oneui_dock_workspace_tests)

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/DockWorkspace.oneview"
    COMMAND oneui-viewc --emit ir --input "${PROJECT_SOURCE_DIR}/tests/workspace/DockWorkspace.one"
        --schema "${PROJECT_SOURCE_DIR}/tests/workspace/DockWorkspace.oneschema"
        --output "${CMAKE_CURRENT_BINARY_DIR}/DockWorkspace.oneview"
    DEPENDS oneui-viewc tests/workspace/DockWorkspace.one tests/workspace/DockWorkspace.oneschema VERBATIM)
add_custom_target(oneui_dock_runtime_ir DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/DockWorkspace.oneview")
add_executable(oneui_dock_runtime_tests tests/workspace/dock_runtime_tests.cpp)
target_link_libraries(oneui_dock_runtime_tests PRIVATE oneui_views oneui_core)
target_include_directories(oneui_dock_runtime_tests PRIVATE src)
target_compile_definitions(oneui_dock_runtime_tests PRIVATE ONEUI_BUILDING_DLL)
add_dependencies(oneui_dock_runtime_tests oneui_dock_runtime_ir)
add_test(NAME oneui_dock_runtime_tests COMMAND oneui_dock_runtime_tests "${CMAKE_CURRENT_BINARY_DIR}/DockWorkspace.oneview")

add_executable(oneui_picker_tests tests/workspace/picker_tests.cpp)
target_link_libraries(oneui_picker_tests PRIVATE oneui_views oneui_core)
target_include_directories(oneui_picker_tests PRIVATE src)
target_compile_definitions(oneui_picker_tests PRIVATE ONEUI_BUILDING_DLL)
add_test(NAME oneui_picker_tests COMMAND oneui_picker_tests)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/Pickers.oneview"
    COMMAND oneui-viewc --emit ir --input "${PROJECT_SOURCE_DIR}/tests/workspace/Pickers.one"
        --schema "${PROJECT_SOURCE_DIR}/tests/workspace/Pickers.oneschema"
        --output "${CMAKE_CURRENT_BINARY_DIR}/Pickers.oneview"
    DEPENDS oneui-viewc tests/workspace/Pickers.one tests/workspace/Pickers.oneschema VERBATIM)
add_custom_target(oneui_picker_runtime_ir DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/Pickers.oneview")
add_executable(oneui_picker_runtime_tests tests/workspace/picker_runtime_tests.cpp)
target_link_libraries(oneui_picker_runtime_tests PRIVATE oneui_views oneui_core)
target_include_directories(oneui_picker_runtime_tests PRIVATE src)
target_compile_definitions(oneui_picker_runtime_tests PRIVATE ONEUI_BUILDING_DLL)
add_dependencies(oneui_picker_runtime_tests oneui_picker_runtime_ir)
add_test(NAME oneui_picker_runtime_tests COMMAND oneui_picker_runtime_tests "${CMAKE_CURRENT_BINARY_DIR}/Pickers.oneview")

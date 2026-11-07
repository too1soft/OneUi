add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/External.oneview"
    COMMAND oneui-viewc --emit ir --input "${PROJECT_SOURCE_DIR}/tests/plugin/External.one"
        --schema "${PROJECT_SOURCE_DIR}/tests/views/Runtime.oneschema"
        --components "${PROJECT_SOURCE_DIR}/tests/plugin/External.onecomponents"
        --name External --output "${CMAKE_CURRENT_BINARY_DIR}/External.oneview"
    DEPENDS oneui-viewc tests/plugin/External.one tests/plugin/External.onecomponents tests/views/Runtime.oneschema
    VERBATIM)
add_custom_target(oneui_external_component_ir DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/External.oneview")
add_executable(oneui_component_tests tests/plugin/component_tests.cpp)
target_include_directories(oneui_component_tests PRIVATE src tests)
target_compile_definitions(oneui_component_tests PRIVATE ONEUI_BUILDING_DLL)
target_link_libraries(oneui_component_tests PRIVATE oneui_views oneui_core oneui)
add_dependencies(oneui_component_tests oneui_external_component_ir)
add_test(NAME oneui_component_tests COMMAND oneui_component_tests
    "${CMAKE_CURRENT_BINARY_DIR}/External.oneview" "${PROJECT_SOURCE_DIR}/tests/plugin/External.onecomponents")

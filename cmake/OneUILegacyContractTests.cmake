# Frozen inputs are copied verbatim. Never regenerate them with current viewc.
set(legacy_contract_directory "${CMAKE_CURRENT_BINARY_DIR}/legacy-contracts")
set(legacy_contract_outputs)
foreach(fixture BaselineInput BaselineCheckbox BaselineTimeSeriesChart BaselineScroll
        BaselineVirtualList BaselineTabs BaselineDockWorkspace BaselineDragHandle
        BaselinePaneHeader BaselineInteractiveSurface TabsV3)
    set(output "${legacy_contract_directory}/${fixture}.oneview")
    add_custom_command(OUTPUT "${output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${legacy_contract_directory}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PROJECT_SOURCE_DIR}/tests/views/legacy/${fixture}.oneview" "${output}"
        DEPENDS "tests/views/legacy/${fixture}.oneview" VERBATIM)
    list(APPEND legacy_contract_outputs "${output}")
endforeach()
foreach(fixture BaselineTimeSeriesChartNew BaselineInteractiveSurfaceNew
        BaselineInteractiveSurfaceClick BaselineScrollNew BaselineTabsNew BaselineNativeHost BaselineVirtualListSlot)
    set(output "${legacy_contract_directory}/${fixture}.oneview")
    add_custom_command(OUTPUT "${output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${legacy_contract_directory}"
        COMMAND oneui-viewc --emit ir --input "${PROJECT_SOURCE_DIR}/tests/views/legacy/${fixture}.one"
            --schema "${PROJECT_SOURCE_DIR}/tests/views/legacy/Baseline.oneschema"
            --name "${fixture}" --output "${output}"
        DEPENDS oneui-viewc "tests/views/legacy/${fixture}.one" tests/views/legacy/Baseline.oneschema tests/views/legacy/LegacySlotShell.one VERBATIM)
    list(APPEND legacy_contract_outputs "${output}")
endforeach()
add_custom_target(oneui_view_legacy_contract_ir DEPENDS ${legacy_contract_outputs})
add_executable(oneui_view_legacy_contract_tests tests/views/legacy_contract_tests.cpp)
target_link_libraries(oneui_view_legacy_contract_tests PRIVATE oneui_views oneui_core)
target_include_directories(oneui_view_legacy_contract_tests PRIVATE src)
target_compile_definitions(oneui_view_legacy_contract_tests PRIVATE ONEUI_BUILDING_DLL)
add_dependencies(oneui_view_legacy_contract_tests oneui_view_legacy_contract_ir)
add_test(NAME oneui_view_legacy_contract_tests COMMAND oneui_view_legacy_contract_tests
    "${legacy_contract_directory}")

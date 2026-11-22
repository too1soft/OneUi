add_executable(oneui_demand_benchmark tests/demand/benchmark.cpp)
target_link_libraries(oneui_demand_benchmark PRIVATE oneui_static)
target_include_directories(oneui_demand_benchmark PRIVATE src)
oneui_target_view(oneui_demand_benchmark NAME Installer
    SOURCE "${PROJECT_SOURCE_DIR}/tests/demand/Installer.one"
    SCHEMA "${PROJECT_SOURCE_DIR}/tests/demand/Installer.oneschema")
if(COMMAND oneui_target_demand)
    oneui_target_demand(oneui_demand_benchmark)
    get_target_property(demand_runtime oneui_demand_benchmark ONEUI_DEMAND_RUNTIME)
    add_executable(oneui_demand_runtime_tests tests/demand/runtime_tests.cpp)
    target_link_libraries(oneui_demand_runtime_tests PRIVATE ${demand_runtime})
    if(ONEUI_STATIC_IPO)
        set_property(TARGET oneui_demand_runtime_tests PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    endif()
    add_test(NAME oneui_demand_runtime_tests COMMAND oneui_demand_runtime_tests)
endif()
add_test(NAME oneui_demand_benchmark COMMAND oneui_demand_benchmark)
add_test(NAME oneui_demand_requirements COMMAND "${Python3_EXECUTABLE}" -B
    "${PROJECT_SOURCE_DIR}/tests/demand/test_requirements.py"
    "$<TARGET_FILE:oneui-viewc>" "${CMAKE_CURRENT_BINARY_DIR}")
add_subdirectory("${PROJECT_SOURCE_DIR}/tests/static_exports" "${CMAKE_CURRENT_BINARY_DIR}/static-export-contract")
add_test(NAME oneui_static_export_contract COMMAND "${Python3_EXECUTABLE}" -B
    "${PROJECT_SOURCE_DIR}/tests/static_exports/verify.py"
    "$<TARGET_FILE:static_consumer>" "$<TARGET_FILE:export_contract>")
if(ONEUI_SKIA_MODE STREQUAL "bundled-static" AND MSVC)
    add_test(NAME oneui_demand_sdk_inputs COMMAND "${Python3_EXECUTABLE}" -B
        "${PROJECT_SOURCE_DIR}/tests/demand/test_sdk_inputs.py"
        "${CMAKE_CURRENT_BINARY_DIR}/sdk-inputs-$<CONFIG>.json")
endif()

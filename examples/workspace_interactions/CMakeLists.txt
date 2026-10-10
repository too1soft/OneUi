# Independent WP4 example. No dependency on, or edits to, performance_lab.
find_program(ONEUI_WORKSPACE_CARGO cargo)
if(ONEUI_WORKSPACE_CARGO)
    add_library(workspace_acceptance STATIC EXCLUDE_FROM_ALL native_acceptance.cpp)
    target_include_directories(workspace_acceptance PRIVATE
        "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/src")
    target_compile_features(workspace_acceptance PRIVATE cxx_std_17)
    target_link_libraries(workspace_acceptance PRIVATE oneui)
    if(MSVC)
        target_compile_options(workspace_acceptance PRIVATE /utf-8)
        set_property(TARGET workspace_acceptance PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
    endif()
    add_custom_target(workspace_interactions
        COMMAND "${CMAKE_COMMAND}" -E env
            "ONEUI_LIB_DIR=$<TARGET_FILE_DIR:oneui>"
            "ONEUI_VIEWC=$<TARGET_FILE:oneui-viewc>"
            "ONEUI_WORKSPACE_BRIDGE_DIR=$<TARGET_FILE_DIR:workspace_acceptance>"
            "CARGO_TARGET_DIR=${PROJECT_SOURCE_DIR}/build/cargo-target"
            "${ONEUI_WORKSPACE_CARGO}" build --release --features acceptance
            --manifest-path "${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml"
        DEPENDS oneui oneui-viewc workspace_acceptance
        USES_TERMINAL VERBATIM)
endif()

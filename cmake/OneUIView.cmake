if(NOT TARGET oneui-viewc)
    add_executable(oneui-viewc "${CMAKE_CURRENT_LIST_DIR}/../tools/viewc/main.cpp")
    target_include_directories(oneui-viewc PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../include")
    target_compile_features(oneui-viewc PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(oneui-viewc PRIVATE /utf-8)
    endif()
endif()

# oneui_target_view(app NAME Settings SOURCE settings.one VM_HEADER vm.h)
function(oneui_target_view target)
    cmake_parse_arguments(VIEW "" "NAME;SOURCE;VM_HEADER" "" ${ARGN})
    if(NOT VIEW_NAME OR NOT VIEW_SOURCE)
        message(FATAL_ERROR "oneui_target_view needs NAME and SOURCE")
    endif()
    get_filename_component(source "${VIEW_SOURCE}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    set(output "${CMAKE_CURRENT_BINARY_DIR}/views/${VIEW_NAME}.g.h")
    add_custom_command(OUTPUT "${output}"
        COMMAND oneui-viewc --input "${source}" --output "${output}" --name "${VIEW_NAME}" --vm-header "${VIEW_VM_HEADER}" --depfile "${output}.d"
        DEPENDS oneui-viewc "${source}"
        DEPFILE "${output}.d"
        VERBATIM)
    target_sources(${target} PRIVATE "${output}")
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/views" "${CMAKE_CURRENT_SOURCE_DIR}")
endfunction()

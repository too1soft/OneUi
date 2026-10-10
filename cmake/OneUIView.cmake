if(NOT TARGET oneui-viewc)
    set(viewc_root "${CMAKE_CURRENT_LIST_DIR}/../tools/viewc")
    add_executable(oneui-viewc
        "${viewc_root}/main.cpp" "${viewc_root}/emit_rust.cpp" "${viewc_root}/emit_cpp.cpp" "${CMAKE_CURRENT_LIST_DIR}/../src/views/format/binary.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../src/views/format/schema.cpp" "${viewc_root}/schema_parser.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../src/views/format/component_contract.cpp" "${viewc_root}/compiler.cpp"
        "${viewc_root}/lower.cpp" "${viewc_root}/lower_attributes.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../src/views/format/validate.cpp" "${CMAKE_CURRENT_LIST_DIR}/../src/views/format/ir_writer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../src/views/format/ir_reader.cpp" "${CMAKE_CURRENT_LIST_DIR}/../src/views/format/ir_validate.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../src/views/format/ir_graph.cpp")
    target_include_directories(oneui-viewc PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../src/views/format" "${CMAKE_CURRENT_LIST_DIR}/../include")
    target_compile_features(oneui-viewc PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(oneui-viewc PRIVATE /utf-8)
    endif()
endif()

# oneui_target_view(app NAME Settings SOURCE settings.one VM_HEADER vm.h)
function(oneui_target_view target)
    cmake_parse_arguments(VIEW "" "NAME;SOURCE;SCHEMA;VM_HEADER" "" ${ARGN})
    if(NOT VIEW_NAME OR NOT VIEW_SOURCE)
        message(FATAL_ERROR "oneui_target_view needs NAME and SOURCE")
    endif()
    get_filename_component(source "${VIEW_SOURCE}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    if(VIEW_SCHEMA)
        get_filename_component(schema "${VIEW_SCHEMA}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    else()
        get_filename_component(directory "${source}" DIRECTORY)
        get_filename_component(stem "${source}" NAME_WE)
        set(schema "${directory}/${stem}.oneschema")
    endif()
    set(output "${CMAKE_CURRENT_BINARY_DIR}/views/${VIEW_NAME}.g.h")
    add_custom_command(OUTPUT "${output}"
        COMMAND oneui-viewc --emit cpp --schema "${schema}" --input "${source}" --output "${output}" --name "${VIEW_NAME}" --vm-header "${VIEW_VM_HEADER}" --depfile "${output}.d"
        DEPENDS oneui-viewc "${source}" "${schema}"
        DEPFILE "${output}.d"
        VERBATIM)
    target_sources(${target} PRIVATE "${output}")
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/views" "${CMAKE_CURRENT_SOURCE_DIR}")
endfunction()

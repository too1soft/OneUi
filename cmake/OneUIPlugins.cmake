include_guard(GLOBAL)

# Build-time registration of explicitly selected, resident plugin packages.
# Target metadata is exported by each plugin's own CMake package.
function(oneui_target_plugins application)
    cmake_parse_arguments(PLUGIN "" "REGISTRAR" "PLUGINS" ${ARGN})
    if(PLUGIN_UNPARSED_ARGUMENTS OR NOT TARGET "${application}")
        message(FATAL_ERROR "oneui_target_plugins requires an existing application target")
    endif()
    if(NOT PLUGIN_REGISTRAR MATCHES "^[A-Za-z_][A-Za-z0-9_]*$")
        message(FATAL_ERROR "Provide a C identifier with REGISTRAR")
    endif()
    get_target_property(_registered "${application}" ONEUI_PLUGIN_REGISTRAR)
    if(_registered)
        message(FATAL_ERROR "Plugin registrar is already configured for ${application}")
    endif()
    set(_pending ${PLUGIN_PLUGINS})
    set(_targets "")
    set(_ids "")
    set(_includes "")
    set(_calls "")
    while(_pending)
        list(POP_FRONT _pending _target)
        if(_target IN_LIST _targets)
            continue()
        endif()
        if(NOT TARGET "${_target}")
            message(FATAL_ERROR "Missing OneUI plugin target: ${_target}")
        endif()
        foreach(_key ID ENTRY HEADER ABI)
            get_target_property(_${_key} "${_target}" "ONEUI_PLUGIN_${_key}")
        endforeach()
        if(NOT _ID MATCHES "^[a-z][a-z0-9_.-]*$" OR
           NOT _ENTRY MATCHES "^[A-Za-z_][A-Za-z0-9_]*$" OR
           NOT _HEADER MATCHES "^[A-Za-z0-9_./-]+$" OR NOT _ABI STREQUAL "1")
            message(FATAL_ERROR "Invalid or incompatible OneUI plugin metadata: ${_target}")
        endif()
        if(_ID IN_LIST _ids)
            message(FATAL_ERROR "Duplicate OneUI plugin ID: ${_ID}")
        endif()
        list(APPEND _ids "${_ID}")
        list(APPEND _targets "${_target}")
        list(LENGTH _targets _count)
        if(_count GREATER 256)
            message(FATAL_ERROR "Too many OneUI plugins")
        endif()
        string(APPEND _includes "#include \"${_HEADER}\"\n")
        string(APPEND _calls
            "    result = oneui_plugin_runtime_register_v1(runtime, ${_ENTRY}());\n"
            "    if (result != ONEUI_PLUGIN_OK) return result;\n")
        get_target_property(_deps "${_target}" ONEUI_PLUGIN_DEPENDENCIES)
        if(_deps)
            list(APPEND _pending ${_deps})
        endif()
    endwhile()
    set(_directory "${CMAKE_CURRENT_BINARY_DIR}/oneui-generated/${PLUGIN_REGISTRAR}")
    file(MAKE_DIRECTORY "${_directory}")
    file(WRITE "${_directory}/oneui_generated_plugins.h"
        "#pragma once\n#include \"oneui/plugin/c_api.h\"\n"
        "#ifdef __cplusplus\nextern \"C\" {\n#endif\n"
        "OneUiPluginResult ${PLUGIN_REGISTRAR}(OneUiPluginRuntime *runtime);\n"
        "#ifdef __cplusplus\n}\n#endif\n")
    file(WRITE "${_directory}/plugins.cpp"
        "#include \"oneui_generated_plugins.h\"\n${_includes}\n"
        "OneUiPluginResult ${PLUGIN_REGISTRAR}(OneUiPluginRuntime *runtime) {\n"
        "    if (!runtime) return ONEUI_PLUGIN_INVALID_ARGUMENT;\n"
        "    OneUiPluginResult result = ONEUI_PLUGIN_OK;\n${_calls}    return result;\n}\n")
    target_sources("${application}" PRIVATE "${_directory}/plugins.cpp")
    target_include_directories("${application}" PRIVATE "${_directory}")
    target_link_libraries("${application}" PRIVATE OneUI::plugin ${_targets})
    set_target_properties("${application}" PROPERTIES ONEUI_PLUGIN_REGISTRAR "${PLUGIN_REGISTRAR}")
endfunction()

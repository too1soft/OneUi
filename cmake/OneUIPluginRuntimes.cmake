include_guard(GLOBAL)

# Follow CMake target dependencies, so consumers do not need to hand-copy the
# DLLs/dylibs of each plugin or its native SDK. System libraries are ignored.
function(_oneui_stage_plugin_runtimes application)
    if(NOT WIN32 AND NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        return()
    endif()
    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        # Imported SDK libraries have @rpath identities. Prefer the staged copy
        # beside the consumer even after its build-time SDK cache is removed.
        set_property(TARGET "${application}" APPEND PROPERTY BUILD_RPATH "@loader_path")
        set_property(TARGET "${application}" APPEND PROPERTY INSTALL_RPATH "@loader_path")
    endif()
    set(_pending ${ARGN})
    set(_visited "")
    while(_pending)
        list(POP_FRONT _pending _target)
        if(_target MATCHES "^\\$<LINK_ONLY:([^>]+)>$")
            set(_target "${CMAKE_MATCH_1}")
        endif()
        if(NOT TARGET "${_target}" OR _target IN_LIST _visited)
            continue()
        endif()
        list(APPEND _visited "${_target}")
        get_target_property(_type "${_target}" TYPE)
        if(_type STREQUAL "SHARED_LIBRARY" OR _type STREQUAL "MODULE_LIBRARY")
            add_custom_command(TARGET "${application}" POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "$<TARGET_FILE:${_target}>" "$<TARGET_FILE_DIR:${application}>"
                VERBATIM)
        endif()
        foreach(_property LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
            get_target_property(_links "${_target}" "${_property}")
            if(_links)
                list(APPEND _pending ${_links})
            endif()
        endforeach()
    endwhile()
endfunction()

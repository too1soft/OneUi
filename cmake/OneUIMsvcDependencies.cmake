# Ninja scans cl's raw /showIncludes output. Older CMake releases decode
# modern MSVC's UTF-8 diagnostics using the console code page, corrupting a
# localized prefix and silently recording zero header dependencies.
if(NOT MSVC OR NOT CMAKE_GENERATOR MATCHES "Ninja")
    return()
endif()

set(_oneui_include_probe "${CMAKE_BINARY_DIR}/CMakeFiles/oneui-include-probe.cpp")
file(WRITE "${_oneui_include_probe}" "#include <stddef.h>\n")
execute_process(
    COMMAND "${CMAKE_CXX_COMPILER}" /nologo /showIncludes /Zs "${_oneui_include_probe}"
    RESULT_VARIABLE _oneui_probe_result
    OUTPUT_VARIABLE _oneui_probe_output
    ERROR_VARIABLE _oneui_probe_error
    ENCODING NONE
    TIMEOUT 20)
if(NOT _oneui_probe_result EQUAL 0)
    message(FATAL_ERROR "Cannot probe MSVC header dependencies: ${_oneui_probe_error}")
endif()
string(REGEX MATCH "(^|\n)([^\r\n]*:[ \t]+)[A-Za-z]:" _oneui_probe_match
    "${_oneui_probe_output}\n${_oneui_probe_error}")
if(NOT _oneui_probe_match)
    message(FATAL_ERROR "MSVC did not expose a usable /showIncludes prefix")
endif()
set(CMAKE_CL_SHOWINCLUDES_PREFIX "${CMAKE_MATCH_2}")
set(CMAKE_CXX_CL_SHOWINCLUDES_PREFIX "${CMAKE_CL_SHOWINCLUDES_PREFIX}")
set(CMAKE_C_CL_SHOWINCLUDES_PREFIX "${CMAKE_CL_SHOWINCLUDES_PREFIX}")

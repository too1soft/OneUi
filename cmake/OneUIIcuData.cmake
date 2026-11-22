# ICU is private to the text engine. Consumers that also use non-text ICU
# services must retain the default full package.
set(ONEUI_ICU_DATA_PROFILE "full" CACHE STRING "Embedded ICU services: full or text")
set_property(CACHE ONEUI_ICU_DATA_PROFILE PROPERTY STRINGS full text)
if(NOT ONEUI_ICU_DATA_PROFILE MATCHES "^(full|text)$")
    message(FATAL_ERROR "ONEUI_ICU_DATA_PROFILE must be full or text")
endif()
if(ONEUI_ICU_DATA_PROFILE STREQUAL "text" AND
        (NOT WIN32 OR NOT ONEUI_SKIA_MODE STREQUAL "bundled-static"))
    message(FATAL_ERROR "The text ICU package currently requires Windows bundled-static Skia")
endif()
function(oneui_prepare_icu_data input output_variable)
    if(ONEUI_ICU_DATA_PROFILE STREQUAL "full")
        set(${output_variable} "${input}" PARENT_SCOPE)
        return()
    endif()
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    set(output "${CMAKE_CURRENT_BINARY_DIR}/generated/icu-text.dat")
    add_custom_command(OUTPUT "${output}"
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/prepare-icu-data.py"
            --input "${input}" --output "${output}" --profile text
            --report "${CMAKE_CURRENT_BINARY_DIR}/generated/icu-text.json"
        DEPENDS "${input}" "${PROJECT_SOURCE_DIR}/scripts/prepare-icu-data.py" VERBATIM)
    set(${output_variable} "${output}" PARENT_SCOPE)
endfunction()

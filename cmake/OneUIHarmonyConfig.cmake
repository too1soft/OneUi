get_filename_component(_oneui_sdk "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "OHOS")
    message(FATAL_ERROR "OneUI HarmonyOS SDK requires the actual OHOS toolchain")
endif()
add_library(OneUI::harmony_application SHARED IMPORTED)
set_target_properties(OneUI::harmony_application PROPERTIES
    IMPORTED_LOCATION "${_oneui_sdk}/lib/liboneui_application.so"
    IMPORTED_SONAME "liboneui_application.so"
    INTERFACE_INCLUDE_DIRECTORIES "${_oneui_sdk}/include"
    INTERFACE_COMPILE_FEATURES cxx_std_17
    INTERFACE_COMPILE_DEFINITIONS "ONEUI_STATIC;ONEUI_MOBILE_STATIC")

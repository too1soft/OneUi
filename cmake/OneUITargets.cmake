add_library(OneUI::oneui SHARED IMPORTED)

get_filename_component(_oneui_sdk_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

if(EXISTS "${_oneui_sdk_root}/lib/oneui.lib")
    set(_oneui_import_lib "${_oneui_sdk_root}/lib/oneui.lib")
elseif(EXISTS "${_oneui_sdk_root}/lib/liboneui.dll.a")
    set(_oneui_import_lib "${_oneui_sdk_root}/lib/liboneui.dll.a")
else()
    message(FATAL_ERROR "OneUI import library was not found under ${_oneui_sdk_root}/lib")
endif()

set_target_properties(OneUI::oneui PROPERTIES
    IMPORTED_LOCATION "${_oneui_sdk_root}/bin/oneui.dll"
    IMPORTED_IMPLIB "${_oneui_import_lib}"
    INTERFACE_INCLUDE_DIRECTORIES "${_oneui_sdk_root}/include"
)

unset(_oneui_sdk_root)
unset(_oneui_import_lib)

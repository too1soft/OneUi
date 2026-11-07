include(CMakeFindDependencyMacro)
find_dependency(OneUI CONFIG)
if(NOT ONEUI_SDK_ABI STREQUAL "46")
    message(FATAL_ERROR "OneUI Status 0.1 requires OneUI SDK ABI 46")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/OneUIStatusTargets.cmake")
get_filename_component(_root "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set_target_properties(OneUI::status PROPERTIES
    ONEUI_COMPONENT_ID oneui.status ONEUI_COMPONENT_ENTRY oneui_status_component_v1
    ONEUI_COMPONENT_HEADER oneui/plugins/status.h
    ONEUI_COMPONENT_CONTRACT "${_root}/share/oneui/plugins/oneui.status/Status.onecomponents")
unset(_root)

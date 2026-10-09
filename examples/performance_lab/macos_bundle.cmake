# One current .app contains its SDK and Rust owner; tests keep linking the SDK
# directly. Signing the complete bundle gives UserNotifications a valid identity.
set(ONEUI_LAB_MACOS_SIGNING_IDENTITY "-" CACHE STRING
    "macOS signing identity; '-' is local ad-hoc signing, not distribution signing")
file(STRINGS "${ONEUI_SOURCE_ROOT}/CMakeLists.txt" lab_sdk_project
    REGEX "^project\\(oneui VERSION [0-9.]+")
string(REGEX MATCH "VERSION ([0-9.]+)" lab_sdk_version "${lab_sdk_project}")
if(NOT CMAKE_MATCH_1)
  message(FATAL_ERROR "Cannot determine the matching OneUI SDK version")
endif()
set_target_properties(oneui-performance-lab PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION "${CMAKE_MATCH_1}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${CMAKE_MATCH_1}"
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "@executable_path/../Frameworks")
set(lab_bundle_script "${CMAKE_CURRENT_LIST_DIR}/macos_sign_bundle.cmake")
set_property(TARGET oneui-performance-lab APPEND PROPERTY LINK_DEPENDS "${lab_bundle_script}")
add_custom_command(TARGET oneui-performance-lab POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
      "-DBUNDLE=$<TARGET_BUNDLE_DIR:oneui-performance-lab>"
      "-DSDK=$<TARGET_FILE:oneui>"
      "-DOWNER=${lab_owner_library}"
      "-DIDENTITY=${ONEUI_LAB_MACOS_SIGNING_IDENTITY}"
      -P "${lab_bundle_script}"
    VERBATIM)

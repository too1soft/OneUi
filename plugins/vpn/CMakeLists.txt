cmake_minimum_required(VERSION 3.16)
project(OneUIVpnPlugin VERSION 0.1.0 LANGUAGES C CXX)
if(NOT TARGET OneUI::plugin)
    find_package(OneUIPlugin CONFIG REQUIRED)
endif()
if(NOT TARGET OneUI::vpn)
    find_package(OneUIVpn CONFIG REQUIRED)
endif()
add_library(oneui_vpn_plugin STATIC src/plugin.cpp)
add_library(OneUI::vpn_plugin ALIAS oneui_vpn_plugin)
target_compile_features(oneui_vpn_plugin PUBLIC cxx_std_17)
target_link_libraries(oneui_vpn_plugin PUBLIC OneUI::plugin OneUI::vpn)
target_include_directories(oneui_vpn_plugin PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
set_target_properties(oneui_vpn_plugin PROPERTIES EXPORT_NAME vpn_plugin POSITION_INDEPENDENT_CODE ON
    ONEUI_PLUGIN_ID "oneui.vpn" ONEUI_PLUGIN_ENTRY "oneui_vpn_plugin_descriptor_v1"
    ONEUI_PLUGIN_HEADER "oneui/plugins/vpn.h" ONEUI_PLUGIN_ABI "1"
    EXPORT_PROPERTIES "ONEUI_PLUGIN_ID;ONEUI_PLUGIN_ENTRY;ONEUI_PLUGIN_HEADER;ONEUI_PLUGIN_ABI")
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
install(TARGETS oneui_vpn_plugin EXPORT OneUIVpnPluginTargets ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(FILES oneui-plugin.json LICENSE DESTINATION "${CMAKE_INSTALL_DATADIR}/oneui/plugins/oneui.vpn")
install(EXPORT OneUIVpnPluginTargets NAMESPACE OneUI:: DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/OneUIVpnPlugin")
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/OneUIVpnPluginConfigVersion.cmake"
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMinorVersion)
install(FILES cmake/OneUIVpnPluginConfig.cmake "${CMAKE_CURRENT_BINARY_DIR}/OneUIVpnPluginConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/OneUIVpnPlugin")

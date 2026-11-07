# Renderer-independent plugin registry and lifecycle ABI.
option(ONEUI_PLUGIN_SHARED "Build the Plugin SDK as a shared library" OFF)
if(ONEUI_PLUGIN_SHARED)
    set(_oneui_plugin_type SHARED)
else()
    set(_oneui_plugin_type STATIC)
endif()
find_package(Threads REQUIRED)
add_library(oneui_plugin ${_oneui_plugin_type}
    src/plugin/c_api.cpp src/plugin/registry.cpp src/plugin/services.cpp src/plugin/lifecycle.cpp)
add_library(OneUI::plugin ALIAS oneui_plugin)
target_compile_features(oneui_plugin PUBLIC cxx_std_17)
target_link_libraries(oneui_plugin PUBLIC Threads::Threads)
target_include_directories(oneui_plugin PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_compile_definitions(oneui_plugin PRIVATE ONEUI_PLUGIN_BUILD)
if(NOT ONEUI_PLUGIN_SHARED)
    target_compile_definitions(oneui_plugin PUBLIC ONEUI_PLUGIN_STATIC)
endif()
set_target_properties(oneui_plugin PROPERTIES EXPORT_NAME plugin POSITION_INDEPENDENT_CODE ON)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
install(TARGETS oneui_plugin EXPORT OneUIPluginTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(DIRECTORY include/oneui/plugin DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/oneui")
install(EXPORT OneUIPluginTargets NAMESPACE OneUI:: DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/OneUIPlugin")
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/OneUIPluginConfigVersion.cmake"
    VERSION 0.1.0 COMPATIBILITY SameMinorVersion)
install(FILES "${CMAKE_CURRENT_LIST_DIR}/OneUIPluginConfig.cmake"
    "${CMAKE_CURRENT_LIST_DIR}/OneUIPlugins.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/OneUIPluginConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/OneUIPlugin")
include("${CMAKE_CURRENT_LIST_DIR}/OneUIPlugins.cmake")
if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    enable_language(C)
    add_executable(oneui_plugin_tests tests/plugin/runtime_tests.cpp)
    target_link_libraries(oneui_plugin_tests PRIVATE OneUI::plugin)
    add_test(NAME oneui_plugin_tests COMMAND oneui_plugin_tests)
    add_executable(oneui_plugin_c_tests tests/plugin/c_api_tests.c)
    target_link_libraries(oneui_plugin_c_tests PRIVATE OneUI::plugin)
    set_target_properties(oneui_plugin_c_tests PROPERTIES LINKER_LANGUAGE CXX)
    add_test(NAME oneui_plugin_c_tests COMMAND oneui_plugin_c_tests)
endif()

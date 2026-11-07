# Renderer-independent plugin registry and lifecycle ABI.
option(ONEUI_PLUGIN_SHARED "Build the Plugin SDK as a shared library" OFF)
if(ONEUI_PLUGIN_SHARED)
    set(_oneui_plugin_type SHARED)
else()
    set(_oneui_plugin_type STATIC)
endif()
find_package(Threads REQUIRED)
if(MSVC AND NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
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
execute_process(COMMAND git -C "${PROJECT_SOURCE_DIR}" rev-parse HEAD
    OUTPUT_VARIABLE _oneui_plugin_commit OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
execute_process(COMMAND git -C "${PROJECT_SOURCE_DIR}" status --porcelain
    OUTPUT_VARIABLE _oneui_plugin_dirty ERROR_QUIET)
if(_oneui_plugin_dirty)
    set(_oneui_plugin_dirty_json true)
else()
    set(_oneui_plugin_dirty_json false)
endif()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/oneui-plugin-build-$<CONFIG>.json" CONTENT
    "{\"schemaVersion\":1,\"sourceCommit\":\"${_oneui_plugin_commit}\",\"sourceDirty\":${_oneui_plugin_dirty_json},\"configuration\":\"$<CONFIG>\",\"system\":\"${CMAKE_SYSTEM_NAME}\",\"processor\":\"${CMAKE_SYSTEM_PROCESSOR}\",\"pointerBytes\":${CMAKE_SIZEOF_VOID_P},\"compiler\":\"${CMAKE_CXX_COMPILER_ID}\",\"compilerVersion\":\"${CMAKE_CXX_COMPILER_VERSION}\",\"runtime\":\"$<IF:$<BOOL:${MSVC}>,$<IF:$<BOOL:${CMAKE_MSVC_RUNTIME_LIBRARY}>,${CMAKE_MSVC_RUNTIME_LIBRARY},default>,system>\",\"linkage\":\"${_oneui_plugin_type}\"}\n")
configure_file("${CMAKE_CURRENT_LIST_DIR}/OneUIPluginBuildInfo.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/OneUIPluginBuildInfo.cmake" @ONLY)
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
    "${CMAKE_CURRENT_LIST_DIR}/OneUIPluginRuntimes.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/OneUIPluginBuildInfo.cmake"
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

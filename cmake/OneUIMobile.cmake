# Optional renderer-independent foundation. No Skia, JNI, Rust or downloads are
# needed for host contract tests. A successful build is not a mobile UI SDK.
option(ONEUI_MOBILE_SHARED "Build the optional mobile runtime as a shared library" OFF)
if(ONEUI_MOBILE_SHARED)
    set(_oneui_mobile_type SHARED)
else()
    set(_oneui_mobile_type STATIC)
endif()
add_library(oneui_mobile_runtime ${_oneui_mobile_type}
    src/mobile/runtime.cpp src/mobile/c_api.cpp
    src/mobile/appearance.cpp src/mobile/appearance_c.cpp)
add_library(OneUI::mobile_runtime ALIAS oneui_mobile_runtime)
find_package(Threads REQUIRED)
target_link_libraries(oneui_mobile_runtime PUBLIC Threads::Threads)
target_include_directories(oneui_mobile_runtime PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_compile_features(oneui_mobile_runtime PUBLIC cxx_std_17)
target_compile_definitions(oneui_mobile_runtime PRIVATE ONEUI_MOBILE_BUILD)
if(NOT ONEUI_MOBILE_SHARED)
    target_compile_definitions(oneui_mobile_runtime PUBLIC ONEUI_MOBILE_STATIC)
endif()
set_target_properties(oneui_mobile_runtime PROPERTIES
    POSITION_INDEPENDENT_CODE ON EXPORT_NAME mobile_runtime)
if(ANDROID)
    target_sources(oneui_mobile_runtime PRIVATE src/platform/android/mobile_input_android.cpp)
    target_link_libraries(oneui_mobile_runtime PUBLIC android)
endif()
option(ONEUI_BUILD_ANDROID_PROBE "Build the Android runtime/GPU integration test host" OFF)
if(ONEUI_BUILD_ANDROID_PROBE)
    if(NOT ANDROID)
        message(FATAL_ERROR "ONEUI_BUILD_ANDROID_PROBE requires the Android NDK toolchain")
    endif()
    add_subdirectory(tests/mobile/android)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
set(_oneui_mobile_package "${CMAKE_INSTALL_LIBDIR}/cmake/OneUIMobile")
configure_package_config_file("${CMAKE_CURRENT_LIST_DIR}/OneUIMobileConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/OneUIMobileConfig.cmake"
    INSTALL_DESTINATION "${_oneui_mobile_package}")
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/OneUIMobileConfigVersion.cmake"
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(TARGETS oneui_mobile_runtime EXPORT OneUIMobileTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES include/oneui/mobile/c_api.h include/oneui/mobile/runtime.h
    include/oneui/mobile/appearance.h include/oneui/mobile/appearance_c.h
    include/oneui/mobile/android.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/oneui/mobile")
install(DIRECTORY platform/android/java/ DESTINATION "${CMAKE_INSTALL_DATADIR}/oneui/android/java")
install(EXPORT OneUIMobileTargets NAMESPACE OneUI:: DESTINATION "${_oneui_mobile_package}")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/OneUIMobileConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/OneUIMobileConfigVersion.cmake"
    DESTINATION "${_oneui_mobile_package}")
if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    enable_language(C)
    add_executable(oneui_mobile_runtime_tests tests/mobile/runtime_tests.cpp)
    target_link_libraries(oneui_mobile_runtime_tests PRIVATE OneUI::mobile_runtime)
    add_test(NAME oneui_mobile_runtime_tests COMMAND oneui_mobile_runtime_tests)
    add_executable(oneui_mobile_c_tests tests/mobile/c_api_tests.c)
    target_link_libraries(oneui_mobile_c_tests PRIVATE OneUI::mobile_runtime)
    set_target_properties(oneui_mobile_c_tests PROPERTIES LINKER_LANGUAGE CXX)
    add_test(NAME oneui_mobile_c_tests COMMAND oneui_mobile_c_tests)
    add_executable(oneui_mobile_appearance_tests tests/mobile/appearance_tests.cpp)
    target_link_libraries(oneui_mobile_appearance_tests PRIVATE OneUI::mobile_runtime)
    add_test(NAME oneui_mobile_appearance_tests COMMAND oneui_mobile_appearance_tests)
    add_executable(oneui_mobile_appearance_c_tests tests/mobile/appearance_c_tests.c)
    target_link_libraries(oneui_mobile_appearance_c_tests PRIVATE OneUI::mobile_runtime)
    set_target_properties(oneui_mobile_appearance_c_tests PROPERTIES LINKER_LANGUAGE CXX)
    add_test(NAME oneui_mobile_appearance_c_tests COMMAND oneui_mobile_appearance_c_tests)
endif()

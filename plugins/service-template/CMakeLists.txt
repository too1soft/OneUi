cmake_minimum_required(VERSION 3.24)
project(ExampleCounter VERSION 0.1.0 LANGUAGES CXX)
find_package(OneUIPlugin CONFIG REQUIRED)
add_library(example_counter STATIC src/plugin.cpp)
add_library(Example::counter ALIAS example_counter)
target_compile_features(example_counter PUBLIC cxx_std_17)
target_link_libraries(example_counter PUBLIC OneUI::plugin)
target_include_directories(example_counter PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)
set_target_properties(example_counter PROPERTIES EXPORT_NAME counter POSITION_INDEPENDENT_CODE ON
    ONEUI_PLUGIN_ID example.counter ONEUI_PLUGIN_ENTRY example_counter_descriptor_v1
    ONEUI_PLUGIN_HEADER example/counter.h ONEUI_PLUGIN_ABI 1
    EXPORT_PROPERTIES "ONEUI_PLUGIN_ID;ONEUI_PLUGIN_ENTRY;ONEUI_PLUGIN_HEADER;ONEUI_PLUGIN_ABI")
include(GNUInstallDirs)
install(TARGETS example_counter EXPORT ExampleCounterTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES oneui-plugin.json LICENSE DESTINATION share/oneui/plugins/example.counter)
install(EXPORT ExampleCounterTargets NAMESPACE Example:: DESTINATION lib/cmake/ExampleCounter)
install(FILES cmake/ExampleCounterConfig.cmake DESTINATION lib/cmake/ExampleCounter)

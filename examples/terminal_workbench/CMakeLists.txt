cmake_minimum_required(VERSION 3.20)
cmake_policy(SET CMP0091 NEW)
project(oneui_terminal_workbench LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ONEUI_BUILD_STATIC OFF CACHE BOOL "" FORCE)
set(ONEUI_ENABLE_YOGA ON CACHE BOOL "" FORCE)
set(ONEUI_ENABLE_TEST_FRAME_CAPTURE ON CACHE BOOL "" FORCE)
set(ONEUI_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." CACHE PATH "OneUI source")
add_subdirectory("${ONEUI_SOURCE_ROOT}" sdk EXCLUDE_FROM_ALL)
include("${ONEUI_SOURCE_ROOT}/cmake/OneUIView.cmake")
find_program(CARGO_EXECUTABLE cargo REQUIRED)
set(rust_target "${CMAKE_BINARY_DIR}/rust-target")
set(rust_dll "${rust_target}/release/oneui_terminal_demo_owner.dll")
set(rust_lib "${rust_dll}.lib")
# Always ask Cargo: it tracks changes in the SDK's Rust path dependencies too.
add_custom_target(terminal-rust-owner
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_LINKER_FILE:oneui>" "$<TARGET_FILE_DIR:oneui>/oneui.lib"
    COMMAND "${CMAKE_COMMAND}" -E env "ONEUI_LIB_DIR=$<TARGET_FILE_DIR:oneui>"
        "${CARGO_EXECUTABLE}" build --release --locked --manifest-path "${CMAKE_CURRENT_SOURCE_DIR}/rust/Cargo.toml" --target-dir "${rust_target}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${rust_dll}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    BYPRODUCTS "${rust_dll}" "${rust_lib}"
    DEPENDS oneui VERBATIM)
add_library(terminal-owner SHARED IMPORTED)
set_target_properties(terminal-owner PROPERTIES IMPORTED_IMPLIB "${rust_lib}" IMPORTED_LOCATION "${rust_dll}")
add_executable(oneui-terminal-workbench main.cpp)
target_link_libraries(oneui-terminal-workbench PRIVATE oneui terminal-owner)
target_compile_options(oneui-terminal-workbench PRIVATE /utf-8 /W3 /wd4251)
target_compile_definitions(oneui-terminal-workbench PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
add_dependencies(oneui-terminal-workbench terminal-rust-owner)
oneui_target_view(oneui-terminal-workbench NAME TerminalWorkbench SOURCE views/TerminalWorkbench.one VM_HEADER vm.h)
enable_testing()
add_test(NAME terminal_workbench COMMAND oneui-terminal-workbench --test)

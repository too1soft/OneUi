# The SDK never includes this file; Rust is required only for the complete lab.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
file(GLOB lab_cpp_examples CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/snippets/cpp/*.cpp")
file(GLOB lab_c_examples CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/snippets/c/*.c")
file(GLOB lab_rust_examples CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/snippets/rust/*.rs")
file(GLOB lab_one_examples CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/snippets/one/*.one")
set(lab_catalog_generator "${ONEUI_SOURCE_ROOT}/scripts/generate-lab-catalog.py")
set(lab_examples_output "${CMAKE_CURRENT_BINARY_DIR}/generated/lab_code_examples.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/capabilities.json" ${lab_catalog_generator}
    ${lab_cpp_examples} ${lab_c_examples} ${lab_rust_examples} ${lab_one_examples})
execute_process(COMMAND "${Python3_EXECUTABLE}" "${lab_catalog_generator}"
    --root "${ONEUI_SOURCE_ROOT}" --output "${lab_examples_output}"
    COMMAND_ERROR_IS_FATAL ANY)
add_library(oneui-lab-compiled-examples OBJECT ${lab_cpp_examples} ${lab_c_examples} snippets/template_check.cpp)
oneui_target_view(oneui-lab-compiled-examples NAME FormSnippet SOURCE snippets/one/FormSnippet.one)
oneui_target_view(oneui-lab-compiled-examples NAME TerminalSnippet SOURCE snippets/one/TerminalSnippet.one)
target_link_libraries(oneui-lab-compiled-examples PRIVATE oneui)
set_target_properties(oneui-lab-compiled-examples PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
if(MSVC)
  target_compile_options(oneui-lab-compiled-examples PRIVATE /utf-8)
endif()
target_sources(oneui-performance-lab PRIVATE "${lab_examples_output}")
target_include_directories(oneui-performance-lab PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
add_dependencies(oneui-performance-lab oneui-lab-compiled-examples)
option(ONEUI_LAB_VERIFY_RUST_EXAMPLES "Compile displayed safe Rust examples with the lab" ON)
if(ONEUI_LAB_VERIFY_RUST_EXAMPLES)
  find_program(lab_cargo cargo REQUIRED)
  if(DEFINED ENV{CARGO_TARGET_DIR})
    set(lab_rust_target_default "$ENV{CARGO_TARGET_DIR}")
  elseif(WIN32)
    set(lab_rust_target_default "${ONEUI_SOURCE_ROOT}/bindings/rust/target")
  else()
    set(lab_rust_target_default "${CMAKE_CURRENT_BINARY_DIR}/rust")
  endif()
  set(ONEUI_LAB_RUST_TARGET_DIR "${lab_rust_target_default}" CACHE PATH "Reusable Rust build output")
  add_custom_target(oneui-lab-rust-examples
    COMMAND "${CMAKE_COMMAND}" -E env "ONEUI_LIB_DIR=$<TARGET_LINKER_FILE_DIR:oneui>"
      "CARGO_TARGET_DIR=${ONEUI_LAB_RUST_TARGET_DIR}" "${lab_cargo}" check
      --manifest-path "${ONEUI_SOURCE_ROOT}/bindings/rust/Cargo.toml" -p oneui-lab-examples
    VERBATIM)
  add_dependencies(oneui-performance-lab oneui-lab-rust-examples)
endif()

"""Relocatable CMake and file inventory for versioned prebuilt SDK packages."""
import hashlib
from pathlib import Path


def digest(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def bytes_hash(data):
    return hashlib.sha256(data).hexdigest()


def cmake_files(platform, architecture, linkage, archives, system, frameworks, version):
    static = linkage == "static"
    library = "oneui_static.lib" if platform == "windows" else "liboneui_static.a"
    runtime = {"windows": "bin/oneui.dll", "linux": "lib/liboneui.so", "macos": "lib/liboneui.dylib"}[platform]
    lines = [
        "include_guard(GLOBAL)",
        'get_filename_component(_ONEUI_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)',
        'set(ONEUI_SDK_LINKAGE "' + linkage + '")',
        'set(ONEUI_SDK_VERSION "' + version + '")',
        "if(TARGET OneUI::oneui)",
        "  return()",
        "endif()",
        "if(NOT CMAKE_SIZEOF_VOID_P EQUAL " + ("4" if architecture == "x86" else "8") + ")",
        '  message(FATAL_ERROR "OneUI SDK architecture does not match this compiler target")',
        "endif()",
    ]
    platform_check = {"windows": "NOT WIN32", "macos": "NOT APPLE", "linux": 'NOT CMAKE_SYSTEM_NAME STREQUAL "Linux"'}[platform]
    lines += ["if(" + platform_check + ")", '  message(FATAL_ERROR "OneUI SDK platform mismatch")', "endif()"]
    if platform == "windows":
        lines += ["if(NOT MSVC)", '  message(FATAL_ERROR "This OneUI SDK requires MSVC")', "endif()",
                  'set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")']
    if platform == "macos" and architecture == "arm64":
        lines += ['if(CMAKE_OSX_ARCHITECTURES AND NOT CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")',
                  '  message(FATAL_ERROR "This OneUI SDK contains arm64 only")', "endif()"]
    lines += ["add_library(OneUI::oneui " + ("STATIC" if static else "SHARED") + " IMPORTED)",
              "set_target_properties(OneUI::oneui PROPERTIES",
              '  IMPORTED_LOCATION "${_ONEUI_ROOT}/' + ("lib/" + library if static else runtime) + '"',
              '  INTERFACE_INCLUDE_DIRECTORIES "${_ONEUI_ROOT}/include"',
              '  INTERFACE_COMPILE_FEATURES "cxx_std_17"',
              '  INTERFACE_COMPILE_OPTIONS "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"',
              ")"]
    if platform == "windows" and not static:
        lines += ['set_property(TARGET OneUI::oneui PROPERTY IMPORTED_IMPLIB "${_ONEUI_ROOT}/lib/oneui.lib")']
    if static:
        lines += ['set_property(TARGET OneUI::oneui PROPERTY INTERFACE_COMPILE_DEFINITIONS "ONEUI_STATIC")']
        libs = ['"${_ONEUI_ROOT}/lib/' + name + '"' for name in archives if name != library]
        if platform == "linux":
            libs = ['"-Wl,--start-group"'] + libs + ['"-Wl,--end-group"']
        libs += ['"' + name + '"' for name in system]
        libs += ['"-framework ' + name + '"' for name in frameworks]
        lines += ['set_property(TARGET OneUI::oneui PROPERTY INTERFACE_LINK_LIBRARIES',
                  "  " + "\n  ".join(libs), ")"]
    lines += ["unset(_ONEUI_ROOT)", ""]
    return "\n".join(lines).encode("utf-8")


def examples_cmake():
    return b'''cmake_minimum_required(VERSION 3.20)
project(oneui_sdk_example LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
find_package(OneUI CONFIG REQUIRED)
add_executable(oneui_cpp main.cpp)
add_executable(oneui_c main.c)
set_target_properties(oneui_c PROPERTIES LINKER_LANGUAGE CXX)
foreach(app IN ITEMS oneui_cpp oneui_c)
  target_link_libraries(${app} PRIVATE OneUI::oneui)
  if(WIN32 AND ONEUI_SDK_LINKAGE STREQUAL "shared")
    add_custom_command(TARGET ${app} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:OneUI::oneui> $<TARGET_FILE_DIR:${app}>)
  endif()
endforeach()
'''


def collect_bindings(source):
    entries = {}
    for crate in ("oneui", "oneui-sys"):
        root = source / "bindings/rust" / crate
        for path in sorted(root.rglob("*")):
            if path.is_file() and not any(part in ("target", ".git") for part in path.relative_to(root).parts):
                entries["bindings/rust/" + crate + "/" + path.relative_to(root).as_posix()] = path
    entries["bindings/rust/Cargo.toml"] = b'[workspace]\nmembers = ["oneui", "oneui-sys"]\nresolver = "2"\n'
    entries["bindings/rust/Cargo.lock"] = source / "bindings/rust/Cargo.lock"
    return entries


def inventory(entries):
    result = []
    for name, value in sorted(entries.items()):
        if isinstance(value, bytes):
            size, hash_value = len(value), bytes_hash(value)
        else:
            size, hash_value = value.stat().st_size, digest(value)
        result.append({"path": name, "size": size, "sha256": hash_value})
    return result

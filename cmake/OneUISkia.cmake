if(WIN32)
    set(_oneui_skia_default "msys2-dynamic")
else()
    set(_oneui_skia_default "bundled-static")
endif()
set(ONEUI_SKIA_MODE "${_oneui_skia_default}" CACHE STRING "Skia integration mode: msys2-dynamic or bundled-static")
set_property(CACHE ONEUI_SKIA_MODE PROPERTY STRINGS "msys2-dynamic" "bundled-static")

if(ONEUI_SKIA_MODE STREQUAL "msys2-dynamic")
    set(SKIA_ROOT "C:/msys64/mingw64" CACHE PATH "Path to the development Skia installation")

    add_library(oneui_skia INTERFACE)
    target_include_directories(oneui_skia
        INTERFACE
            ${SKIA_ROOT}/include/skia
    )
    target_link_directories(oneui_skia
        INTERFACE
            ${SKIA_ROOT}/lib
    )
    target_link_libraries(oneui_skia
        INTERFACE
            skia
    )
elseif(ONEUI_SKIA_MODE STREQUAL "bundled-static")
    if(MSVC)
        # The bundled Skia archives are built with the static MSVC runtime.
        # Match it for every OneUI target in this build to avoid CRT mixing.
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()

    set(ONEUI_BUNDLED_SKIA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/skia" CACHE PATH "Path to vendored Skia source")
    if(WIN32)
        set(_oneui_skia_out "oneui-win-x64-release")
    else()
        string(TOLOWER "${CMAKE_SYSTEM_NAME}" _oneui_os)
        set(_oneui_skia_out "oneui-${_oneui_os}-${CMAKE_SYSTEM_PROCESSOR}-release")
    endif()
    set(ONEUI_BUNDLED_SKIA_OUT "${ONEUI_BUNDLED_SKIA_ROOT}/out/${_oneui_skia_out}" CACHE PATH "Path to vendored Skia build output")
    set(ONEUI_BUNDLED_SKIA_LIB "" CACHE FILEPATH "Path to static Skia library")

    if(NOT EXISTS "${ONEUI_BUNDLED_SKIA_ROOT}/include/core/SkCanvas.h")
        message(FATAL_ERROR
            "Bundled Skia headers were not found. Expected: "
            "${ONEUI_BUNDLED_SKIA_ROOT}/include/core/SkCanvas.h. "
            "Fetch/build Skia with scripts/build-skia-static.ps1.")
    endif()

    if(ONEUI_BUNDLED_SKIA_LIB STREQUAL "")
        if(EXISTS "${ONEUI_BUNDLED_SKIA_OUT}/skia.lib")
            set(ONEUI_BUNDLED_SKIA_LIB "${ONEUI_BUNDLED_SKIA_OUT}/skia.lib")
        elseif(EXISTS "${ONEUI_BUNDLED_SKIA_OUT}/libskia.a")
            set(ONEUI_BUNDLED_SKIA_LIB "${ONEUI_BUNDLED_SKIA_OUT}/libskia.a")
        elseif(EXISTS "${ONEUI_BUNDLED_SKIA_OUT}/libskia.lib")
            set(ONEUI_BUNDLED_SKIA_LIB "${ONEUI_BUNDLED_SKIA_OUT}/libskia.lib")
        endif()
    endif()

    if(NOT EXISTS "${ONEUI_BUNDLED_SKIA_LIB}")
        message(FATAL_ERROR
            "Bundled static Skia library was not found. Expected: "
            "${ONEUI_BUNDLED_SKIA_OUT}/skia.lib or ${ONEUI_BUNDLED_SKIA_OUT}/libskia.a. "
            "Build it with scripts/build-skia-static.ps1.")
    endif()

    add_library(oneui_skia STATIC IMPORTED GLOBAL)
    set_target_properties(oneui_skia PROPERTIES
        IMPORTED_LOCATION "${ONEUI_BUNDLED_SKIA_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${ONEUI_BUNDLED_SKIA_ROOT}"
    )

    # Newer Skia exposes gradients through SkGradient/SkShaders. Keep OneUI's
    # source compatible with both API generations without affecting the
    # existing MSYS2 dynamic build.
    if(NOT EXISTS "${ONEUI_BUNDLED_SKIA_ROOT}/include/effects/SkGradientShader.h")
        target_include_directories(oneui_skia BEFORE INTERFACE
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/skia_compat"
        )
    endif()

    # Reviewed closure for the pinned GN configuration. Never silently absorb
    # arbitrary archives left in an output directory by another build.
    set(_oneui_skia_dependencies skparagraph skshaper skunicode_icu skunicode_core
        harfbuzz icu expat libjpeg libjpeg12 libjpeg16 libpng libwebp skcms zlib)
    if(NOT WIN32 AND NOT APPLE)
        list(APPEND _oneui_skia_dependencies freetype2)
    endif()
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|amd64|x86_64|x64|i.86)$" OR (WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8))
        list(APPEND _oneui_skia_dependencies libwebp_sse41)
    endif()
    foreach(_name IN LISTS _oneui_skia_dependencies)
        if(MSVC)
            set(_archive "${ONEUI_BUNDLED_SKIA_OUT}/${_name}.lib")
        else()
            string(REGEX REPLACE "^lib" "" _unix_name "${_name}")
            set(_archive "${ONEUI_BUNDLED_SKIA_OUT}/lib${_unix_name}.a")
        endif()
        if(NOT EXISTS "${_archive}")
            message(FATAL_ERROR "Required pinned Skia dependency missing: ${_archive}")
        endif()
        target_link_libraries(oneui_skia INTERFACE "${_archive}")
    endforeach()
    # Resolve the reverse text-module -> core references with Unix one-pass linkers.
    target_link_libraries(oneui_skia INTERFACE "${ONEUI_BUNDLED_SKIA_LIB}")
else()
    message(FATAL_ERROR "Unknown ONEUI_SKIA_MODE: ${ONEUI_SKIA_MODE}")
endif()

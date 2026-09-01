set(ONEUI_SKIA_MODE "msys2-dynamic" CACHE STRING "Skia integration mode: msys2-dynamic or bundled-static")
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
    set(ONEUI_BUNDLED_SKIA_OUT "${ONEUI_BUNDLED_SKIA_ROOT}/out/oneui-win-x64-release" CACHE PATH "Path to vendored Skia build output")
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

    file(GLOB _oneui_skia_static_libs
        "${ONEUI_BUNDLED_SKIA_OUT}/*.lib"
        "${ONEUI_BUNDLED_SKIA_OUT}/*.a"
    )
    list(REMOVE_ITEM _oneui_skia_static_libs "${ONEUI_BUNDLED_SKIA_LIB}")
    if(_oneui_skia_static_libs)
        target_link_libraries(oneui_skia INTERFACE ${_oneui_skia_static_libs})
    endif()

    if(EXISTS "${ONEUI_BUNDLED_SKIA_OUT}/oneui-skia-libs.cmake")
        include("${ONEUI_BUNDLED_SKIA_OUT}/oneui-skia-libs.cmake")
    endif()
else()
    message(FATAL_ERROR "Unknown ONEUI_SKIA_MODE: ${ONEUI_SKIA_MODE}")
endif()

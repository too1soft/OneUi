add_executable(oneui_embedded_font_profile_tests tests/embedded_font_profile_tests.cpp)
target_include_directories(oneui_embedded_font_profile_tests PRIVATE src include tests)
target_link_libraries(oneui_embedded_font_profile_tests PRIVATE oneui_text)
target_compile_definitions(oneui_embedded_font_profile_tests PRIVATE
    ONEUI_TEXT_ASSETS="${ONEUI_TEXT_TEST_ASSET_ROOT}")
if(MSVC)
    target_compile_options(oneui_embedded_font_profile_tests PRIVATE /GR-)
endif()
add_test(NAME oneui_embedded_font_profile_tests COMMAND oneui_embedded_font_profile_tests)
oneui_add_core_test(oneui_image_view_codec_tests tests/image_view_codec_tests.cpp)
target_sources(oneui_image_view_codec_tests PRIVATE
    src/platform/shared/skia_canvas.cpp src/platform/shared/skia_path.cpp)
target_include_directories(oneui_image_view_codec_tests PRIVATE src include)
target_compile_features(oneui_image_view_codec_tests PRIVATE cxx_std_20)
if(WIN32 AND ONEUI_SKIA_MODE STREQUAL "bundled-static")
    add_executable(oneui_icu_text_profile_tests tests/icu_text_profile_tests.cpp)
    target_include_directories(oneui_icu_text_profile_tests PRIVATE
        "${ONEUI_BUNDLED_SKIA_ROOT}/third_party/externals/icu/source/common")
    target_compile_definitions(oneui_icu_text_profile_tests PRIVATE U_STATIC_IMPLEMENTATION U_DISABLE_RENAMING=1)
    target_link_libraries(oneui_icu_text_profile_tests PRIVATE oneui_skia)
    add_test(NAME oneui_icu_data_profile_tests COMMAND "${Python3_EXECUTABLE}" -B
        "${PROJECT_SOURCE_DIR}/tests/icu_data_profile_tests.py"
        --input "${ONEUI_BUNDLED_SKIA_ROOT}/third_party/externals/icu/common/icudtl.dat"
        --output "${CMAKE_CURRENT_BINARY_DIR}/icu-profile-tests"
        --native "$<TARGET_FILE:oneui_icu_text_profile_tests>")
endif()

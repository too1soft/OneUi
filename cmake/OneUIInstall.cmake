# Dynamic SDK only. Static archives and Skia remain separate source-build products.
install(TARGETS oneui LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
install(TARGETS oneui_gallery BUNDLE DESTINATION examples/gallery RUNTIME DESTINATION bin)
if(APPLE)
    install(FILES "$<TARGET_FILE:oneui>"
        DESTINATION examples/gallery/oneui_gallery.app/Contents/Frameworks)
endif()
install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/oneui" DESTINATION include)
install(FILES "${PROJECT_SOURCE_DIR}/cmake/OneUIConfig.cmake"
    "${PROJECT_SOURCE_DIR}/cmake/OneUITargets.cmake" DESTINATION cmake)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/docs/" DESTINATION docs FILES_MATCHING PATTERN "*.md")
install(FILES "${PROJECT_SOURCE_DIR}/README.md" "${PROJECT_SOURCE_DIR}/README_EN.md"
    "${PROJECT_SOURCE_DIR}/LICENSE" DESTINATION . OPTIONAL)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/bindings/rust/" DESTINATION bindings/rust
    PATTERN "target" EXCLUDE PATTERN ".git" EXCLUDE)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/website/" DESTINATION website
    PATTERN "node_modules" EXCLUDE PATTERN ".nuxt" EXCLUDE PATTERN ".output" EXCLUDE
    PATTERN ".cache" EXCLUDE PATTERN ".git" EXCLUDE PATTERN "screenshots" EXCLUDE
    PATTERN "test-results" EXCLUDE PATTERN "playwright-report" EXCLUDE
    PATTERN ".screenshots" EXCLUDE PATTERN ".data" EXCLUDE
    PATTERN "dist" EXCLUDE PATTERN ".DS_Store" EXCLUDE)

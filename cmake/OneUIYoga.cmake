# Experimental, opt-in native Flexbox backend. Never downloads for legacy builds.
option(ONEUI_ENABLE_YOGA "Enable the experimental Yoga Stack backend" OFF)
if(ONEUI_ENABLE_YOGA)
    include(FetchContent)
    FetchContent_Declare(oneui_yoga
        URL https://codeload.github.com/react/yoga/tar.gz/042f5013152eb81c1552dec945b88f7b95ca350f
        URL_HASH SHA256=4742f41722a16f181e3da37abf943390db1e928f00f26402cb154662ae7f110f)
    FetchContent_GetProperties(oneui_yoga)
    if(NOT oneui_yoga_POPULATED)
        FetchContent_Populate(oneui_yoga)
    endif()
    configure_file("${oneui_yoga_SOURCE_DIR}/LICENSE" "${CMAKE_BINARY_DIR}/licenses/Yoga-LICENSE.txt" COPYONLY)
    set(ONEUI_YOGA_SOURCE "${oneui_yoga_SOURCE_DIR}")
    include("${CMAKE_CURRENT_LIST_DIR}/../scripts/patch-yoga-wrap.cmake")
    # Use only the core; upstream's root also configures its own tests/tools.
    add_subdirectory("${oneui_yoga_SOURCE_DIR}/yoga" "${oneui_yoga_BINARY_DIR}" EXCLUDE_FROM_ALL)
    set_target_properties(yogacore PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_link_libraries(oneui_core PUBLIC yogacore)
    target_compile_definitions(oneui_core PUBLIC ONEUI_HAS_YOGA=1)
endif()

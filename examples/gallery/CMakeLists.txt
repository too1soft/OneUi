add_executable(oneui_gallery
    gallery_view.cpp
    main.cpp
)

target_link_libraries(oneui_gallery
    PRIVATE
        oneui
)

target_compile_definitions(oneui_gallery
    PRIVATE
        UNICODE
        _UNICODE
)

if(MINGW)
    target_link_options(oneui_gallery
        PRIVATE
            -static-libgcc
            -static-libstdc++
    )
    target_link_libraries(oneui_gallery
        PRIVATE
            -Wl,-Bstatic
            winpthread
            -Wl,-Bdynamic
    )
endif()

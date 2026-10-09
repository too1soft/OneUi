add_library(oneui_mobile_probe SHARED host.cpp egl_surface.cpp)
target_link_libraries(oneui_mobile_probe PRIVATE OneUI::mobile_runtime android log EGL GLESv2)
target_compile_options(oneui_mobile_probe PRIVATE -Wall -Wextra -Werror)
target_link_options(oneui_mobile_probe PRIVATE -Wl,--no-undefined)

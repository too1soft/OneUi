#include <oneui/platform/window.h>

bool example_menus(oneui::Window &window) {
    oneui::NativeMenuItem file{0, L"文件"};
    file.children = {{101, L"打开"}, {102, L"退出"}};
    window.setOnMenuAction([](int command) {
        // Dispatch stable command IDs; update controlled checked/radio state.
        (void)command;
    });
    const bool menuAccepted = window.setMenuBar({file});
    // Tray residence is an explicit choice, and availability depends on the OS.
    window.setTrayMenu({{101, L"打开"}, {102, L"退出"}});
    return menuAccepted;
}

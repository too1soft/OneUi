#include "oneui/oneui_c_api.h"
#include <iostream>
#include <thread>
namespace {
int failures = 0;
void check(bool v, const char *m) {
    if (!v) {
        ++failures;
        std::cerr << m << "\n";
    }
}
} // namespace
int main() {
    OneUiWindowOptions options{};
    options.title = L"Menu ABI regression";
    options.width = 320;
    options.height = 200;
    auto *window = oneui_window_create(&options);
    check(window, "window created");
    if (!window)
        return 1;
    OneUiNativeMenuItemUtf8 item{};
    item.id = 42;
    item.text = "保存";
    item.flags = OneUiNativeMenuEnabled | OneUiNativeMenuCheck | OneUiNativeMenuChecked;
    item.shortcut_key = "s";
    item.shortcut_modifiers = 16;
    OneUiNativeMenuItemUtf8 root{};
    root.text = "文件";
    root.flags = OneUiNativeMenuEnabled;
    root.children = &item;
    root.child_count = 1;
    check(oneui_window_set_menu_bar_utf8(window, &root, 1) == 1,
          "nested UTF-8 menu accepted before native initialization");
    check(oneui_window_set_tray_menu_utf8(window, &root, 1) == 1, "same tree accepted for tray");
    check(oneui_window_capabilities(window) & OneUiWindowCapabilityMenuBar,
          "menu bar capability advertised");
    item.flags |= OneUiNativeMenuRadio;
    check(!oneui_window_set_menu_bar_utf8(window, &root, 1), "ambiguous check flags rejected");
    item.flags = OneUiNativeMenuEnabled;
    root.children = nullptr;
    check(!oneui_window_set_menu_bar_utf8(window, &root, 1), "missing children rejected");
    root.children = &root;
    check(!oneui_window_set_menu_bar_utf8(window, &root, 1), "cyclic C tree is bounded");
    root.children = &item;
    item.text = "\xff";
    check(!oneui_window_set_menu_bar_utf8(window, &root, 1), "malformed UTF-8 rejected");
    item.text = "Save";
    check(!oneui_window_set_tray_menu_utf8(window, &item, 1025),
          "oversized count rejected before reading");
    item.id = 0xE001;
    check(!oneui_window_set_tray_menu_utf8(window, &item, 1), "reserved IDs rejected");
    item.id = 42;
    int wrongThread = 1;
    std::thread worker([&] { wrongThread = oneui_window_set_menu_bar_utf8(window, &root, 1); });
    worker.join();
    check(!wrongThread, "worker thread rejected without crossing C exception boundary");
    unsigned char pixels[12] = {255, 0, 0, 255, 1, 2, 3, 4, 0, 255, 0, 128};
    check(oneui_window_set_tray_icon_rgba(window, pixels, 1, 2, 8, 1) == 1,
          "padded RGBA stride copied");
    check(!oneui_window_set_tray_icon_rgba(window, pixels, 2, 2, 4, 0), "short stride rejected");
    check(!oneui_window_set_tray_icon_rgba(window, pixels, 257, 1, 1028, 0),
          "oversized icon rejected");
    check(oneui_window_set_tray_icon_rgba(window, nullptr, 0, 0, 0, 0) == 1,
          "default icon restored");
    check(oneui_window_set_menu_bar_utf8(window, nullptr, 0) == 1, "application menu removed");
    check(oneui_window_set_tray_menu_utf8(window, nullptr, 0) == 1, "default tray restored");
    oneui_window_destroy(window);
    return failures ? 1 : 0;
}

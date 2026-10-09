#include <oneui/platform/window.h>

void example_files(oneui::Window &window) {
    oneui::SystemClipboard clipboard;
    clipboard.setText(L"你好 OneUI");
    window.setFileDropHandler([](std::vector<std::wstring> paths, oneui::Point point) {
        // Paths are values. This example does not open or execute dropped files.
        (void)paths;
        (void)point;
    });
    // Clear the handler on page deactivation: window.setFileDropHandler({}).
}

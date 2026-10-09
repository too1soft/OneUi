#include <oneui/developer_tools.h>
#include <oneui/platform/window.h>

std::string example_diagnostics(oneui::Window &window) {
    window.enableDeveloperTools();
    // Production builds may disable developer tools. Inspect runtime support
    // before presenting this as an available feature to the user.
    return oneui::developerReportJson(window.developerSnapshot());
}

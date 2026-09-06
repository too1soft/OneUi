#include "oneui/platform/monitor.h"
#include "oneui/platform/window.h"

#include <iostream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

int failures = 0;

void expectTrue(const char* name, bool value) {
    if (!value) {
        std::cerr << name << ": expected true\n";
        ++failures;
    }
}

void testMonitorEnumerationHasStableShape() {
    const auto monitors = oneui::enumerateMonitors();
    expectTrue("Monitor enumeration is not empty", !monitors.empty());

    bool hasPrimary = false;
    for (std::size_t index = 0; index < monitors.size(); ++index) {
        const oneui::MonitorInfo& monitor = monitors[index];
        expectTrue("Monitor index is sequential", monitor.index == static_cast<int>(index));
        expectTrue("Monitor width is non-negative", monitor.bounds.width >= 0.0f);
        expectTrue("Monitor height is non-negative", monitor.bounds.height >= 0.0f);
        expectTrue("Monitor work area width is non-negative", monitor.workArea.width >= 0.0f);
        expectTrue("Monitor work area height is non-negative", monitor.workArea.height >= 0.0f);
        expectTrue("Monitor scale is positive", monitor.scale > 0.0f);
        hasPrimary = hasPrimary || monitor.primary;
    }

    expectTrue("Monitor enumeration has a primary monitor", hasPrimary);
}

#ifdef _WIN32
void testCloseToTrayOwnsARecoverableTrayEntry() {
    auto window = oneui::Window::create(oneui::WindowOptions{L"OneUI tray test", 320, 180, false});
    expectTrue("Tray test window creates", window != nullptr);
    if (!window) {
        return;
    }
    window->initialize();
    window->setCloseToTray(true);
    if (GetShellWindow() != nullptr) {
        expectTrue("Close-to-tray creates a tray icon", window->trayIconVisible());
    }
    window->setCloseToTray(false);
    expectTrue("Disabling close-to-tray removes the tray icon", !window->trayIconVisible());
    window->close();
}
#endif

} // namespace

int main() {
    testMonitorEnumerationHasStableShape();
#ifdef _WIN32
    testCloseToTrayOwnsARecoverableTrayEntry();
#endif

    if (failures != 0) {
        std::cerr << failures << " monitor behavior test(s) failed.\n";
        return 1;
    }

    return 0;
}

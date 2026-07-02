#include "oneui/platform/monitor.h"

#include <iostream>

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

} // namespace

int main() {
    testMonitorEnumerationHasStableShape();

    if (failures != 0) {
        std::cerr << failures << " monitor behavior test(s) failed.\n";
        return 1;
    }

    return 0;
}

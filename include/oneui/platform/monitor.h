#pragma once

#include "oneui/export.h"
#include "oneui/geometry.h"

#include <string>
#include <vector>

namespace oneui {

struct MonitorInfo {
    int index = 0;
    Rect bounds;
    Rect workArea;
    float scale = 1.0f;
    bool primary = false;
    std::wstring name;
};

ONEUI_API std::vector<MonitorInfo> enumerateMonitors();

} // namespace oneui

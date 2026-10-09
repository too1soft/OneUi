#pragma once

// CMake defines this consistently for every source in a configuration. Sources
// built outside CMake default to production behavior; opt in explicitly to tools.
#ifndef ONEUI_DEVELOPER_TOOLS
#define ONEUI_DEVELOPER_TOOLS 0
#endif

#pragma once

// Only source-built, explicitly specialized runtimes define this header.
// Full SDKs and existing dynamic/IR consumers keep every component available.
#ifdef ONEUI_COMPONENT_SELECTION_HEADER
#include ONEUI_COMPONENT_SELECTION_HEADER
#define ONEUI_COMPONENT_ENABLED(name) ONEUI_COMPONENT_ENABLED_##name
#else
#define ONEUI_COMPONENT_ENABLED(name) 1
namespace oneui::ui::detail {
inline bool componentSelected(const char*) { return true; }
}
#endif

#define ONEUI_COMPONENT_IS(value, name) (ONEUI_COMPONENT_ENABLED(name) && (value) == #name)

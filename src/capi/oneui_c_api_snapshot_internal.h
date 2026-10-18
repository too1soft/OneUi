#pragma once

#include "oneui_c_api_widget_internal.h"
#include <optional>
#include <unordered_map>

namespace oneui::capi {
struct DebugStyleBinding {
    std::string tag;
    std::vector<std::string> classes;
    std::shared_ptr<oneui::StyleSheet> styleSheet;
    std::optional<oneui::StyleBox> resolved;
};

using DebugStyleMap = std::unordered_map<const oneui::Widget*, DebugStyleBinding>;

DebugStyleMap debugStyleBindings();
} // namespace oneui::capi

#pragma once
#include <oneui/application.h>
#include <optional>

namespace lab {
oneui::ApplicationOptions applicationIdentity();
std::optional<int> desktopIntegrationCommand(const std::vector<std::wstring> &arguments);
} // namespace lab

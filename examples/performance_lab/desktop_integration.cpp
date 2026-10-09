#include "desktop_integration.hpp"
#include <filesystem>
#include <iostream>

namespace lab {
oneui::ApplicationOptions applicationIdentity() {
    return {"org.oneui.performance-lab", L"OneUI 试验台"};
}
std::optional<int> desktopIntegrationCommand(const std::vector<std::wstring> &arguments) {
    if (arguments.size() != 2)
        return {};
    const bool install = arguments[1] == L"--install-desktop";
    if (!install && arguments[1] != L"--uninstall-desktop")
        return {};
    const auto result =
        install
            ? oneui::Application::installDesktopIntegration(
                  applicationIdentity(), std::filesystem::canonical(arguments.front()).wstring())
            : oneui::Application::uninstallDesktopIntegration(applicationIdentity());
    std::cout << result.detail << '\n';
    return result.status == oneui::NotificationStatus::Accepted ? 0 : 1;
}
} // namespace lab

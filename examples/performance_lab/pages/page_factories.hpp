#pragma once
#include "../demo_page.hpp"
namespace lab {
std::unique_ptr<DemoPage> createBasicLayoutPage();
std::unique_ptr<DemoPage> createBasicControlsPage();
std::unique_ptr<DemoPage> createProductPage();
std::unique_ptr<DemoPage> createRemotePage(oneui::Window &);
std::unique_ptr<DemoPage> createTerminalPage(oneui::Window &);
std::unique_ptr<DemoPage> createMultiWindowPage(oneui::Window &);
std::unique_ptr<DemoPage> createNotificationsPage();
std::unique_ptr<DemoPage> createDockPage(oneui::Window&);
std::unique_ptr<DemoPage> createWorkspacePage(const PageDescriptor &, oneui::Window &);
std::unique_ptr<DemoPage> createUtilityPage(const PageDescriptor &, oneui::Window &);
std::unique_ptr<DemoPage> createLayoutPage(const PageDescriptor &, oneui::Window &);
std::unique_ptr<DemoPage> createControlPage(const PageDescriptor &, oneui::Window &);
std::unique_ptr<DemoPage> createDesktopPage(const PageDescriptor &, oneui::Window &);
std::unique_ptr<DemoPage> createMediaPage(const PageDescriptor &, oneui::Window &);
std::unique_ptr<DemoPage> createPerformanceValidationPage(bool stability);
std::unique_ptr<DemoPage> createPerformancePage(const PageDescriptor &, oneui::Window &);
} // namespace lab

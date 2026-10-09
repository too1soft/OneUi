#include "page_factories.hpp"
#include <stdexcept>

namespace lab {
std::unique_ptr<DemoPage> createDemoPage(const PageDescriptor &page, oneui::Window &window) {
    switch (page.kind) {
    case PageKind::Layout:
    case PageKind::Recipes:
        return createLayoutPage(page, window);
    case PageKind::Workspace:
        return createWorkspacePage(page, window);
    case PageKind::MultiWindow:
        return createMultiWindowPage(window);
    case PageKind::Dock:
        return createDockPage(window);
    case PageKind::Performance:
        return createPerformancePage(page, window);
    case PageKind::Controls:
    case PageKind::Text:
    case PageKind::Data:
        return createControlPage(page, window);
    case PageKind::Terminal:
        return createTerminalPage(window);
    case PageKind::Drawing:
        return createMediaPage(page, window);
    case PageKind::Remote:
        return createRemotePage(window);
    case PageKind::Notifications:
    case PageKind::Menus:
    case PageKind::Windows:
        return createDesktopPage(page, window);
    case PageKind::Files:
    case PageKind::Reactive:
    case PageKind::Theme:
    case PageKind::Diagnostics:
        return createUtilityPage(page, window);
    }
    throw std::invalid_argument("Page has no factory");
}
} // namespace lab

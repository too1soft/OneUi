#include <oneui/controls/label.h>
#include <oneui/controls/text_field.h>
#include <oneui/layout/app_shell.h>
#include <oneui/layout/top_bar.h>

// The shell retains each slot; the same editor survives sidebar changes.
std::shared_ptr<oneui::AppShell> example_product() {
    using namespace oneui;
    auto shell = std::make_shared<AppShell>();
    auto toolbar = std::make_shared<TopBar>();
    toolbar->setLeading(std::make_shared<Label>(L"本地工作区"));
    toolbar->setLeadingWidth(240);
    shell->setHeader(toolbar);
    shell->setHeaderHeight(48);
    shell->setSidebar(std::make_shared<Label>(L"设备列表"));
    shell->setSidebarWidth(180);
    auto editor = std::make_shared<TextField>();
    editor->setText(L"保留的草稿");
    shell->setContent(editor);
    shell->setFooter(std::make_shared<Label>(L"本地模拟 · 无网络连接"));
    shell->setFooterHeight(28);
    shell->setFooterSpanSidebar(true);
    // Call setSidebarVisible(width >= 760) from your host's size callback.
    // This changes layout without recreating or clearing the editor.
    return shell;
}

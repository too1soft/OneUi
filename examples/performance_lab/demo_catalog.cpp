#include "demo_catalog.hpp"
#include <algorithm>

namespace lab {
const wchar_t *categoryName(Category category) {
    switch (category) {
    case Category::Layout:
        return L"布局";
    case Category::Performance:
        return L"性能";
    case Category::Features:
        return L"功能";
    }
    return L"";
}
const std::vector<PageDescriptor> &pageCatalog() {
    static const std::vector<PageDescriptor> pages{
        {"layout/basic", Category::Layout, L"基础布局", L"调整方向、间距和空间分配，观察原生布局。",
         PageKind::Layout, 0, "Stack Grid Wrap Panel ScrollView"},
        {"layout/forms", Category::Layout, L"表单与自适应", L"字段换行、校验、密度与窄窗口。",
         PageKind::Recipes, 0, "FormGrid FormRow SettingsPage"},
        {"layout/pages", Category::Layout, L"完整页面组合", L"应用外壳、模拟设备连接、列表与详情；切换保留编辑状态。",
         PageKind::Recipes, 4, "ListPage DetailPage MasterDetail SidebarLayout"},
        {"layout/workspace", Category::Layout, L"工作区分栏",
         L"标签、终端、文件与监控组成的本地工作区。", PageKind::Workspace, 0,
         "Workspace DockPanel SplitView Tabs NativeHost"},
        {"layout/docking", Category::Layout, L"停靠与浮动", L"标签排序、边缘分栏与独立系统窗口。",
         PageKind::Dock, 0, "DockWorkspace DockSession DockSurface FloatingFrame"},
        {"performance/overview", Category::Performance, L"综合基准",
         L"同一窗口中的图表、粒子和虚拟列表。", PageKind::Performance, 0,
         "RendererInfo frameProfile"},
        {"performance/charts", Category::Performance, L"实时图表", L"曲线、频谱与绘制采样。",
         PageKind::Performance, 1, "TimeSeriesChart Canvas paths"},
        {"performance/particles", Category::Performance, L"粒子绘制",
         L"相同负载下比较图元绘制路径。", PageKind::Performance, 2, "Canvas circleBatch"},
        {"performance/lists", Category::Performance, L"大数据滚动", L"虚拟化和可见区域绘制。",
         PageKind::Performance, 3, "VirtualList Table"},
        {"performance/materials", Category::Performance, L"文字与材质",
         L"渐变、阴影、段落与文字布局。", PageKind::Recipes, 3, "TextLayout Gradient Shadow"},
        {"performance/idle", Category::Performance, L"动效与空闲", L"有限动效结束后停止重绘。",
         PageKind::Recipes, 3, "Reveal transitions reducedMotion"},
        {"performance/renderers", Category::Performance, L"CPU / GPU 对照",
         L"独立运行相同场景，核对实际后端、物理尺寸与绘制耗时。", PageKind::Performance, 4,
         "RendererInfo RendererMemoryInfo"},
        {"performance/stability", Category::Performance, L"稳定性测试",
         L"持续采样及资源生命周期验证。", PageKind::Performance, 5, "RendererSoak lifecycle"},
        {"features/controls", Category::Features, L"基础控件", L"按钮、选择与进度的真实事件。",
         PageKind::Controls, 0,
         "Button IconButton Checkbox Switch RadioGroup Slider Select ProgressBar"},
        {"features/text", Category::Features, L"文字输入与字体",
         L"中文、字素编辑、富文本、剪贴板和字体。", PageKind::Text, 0,
         "TextField TextArea Label RichText Fonts IME"},
        {"features/data", Category::Features, L"列表、表格与树", L"稳定 ID、选择、展开与重排。",
         PageKind::Data, 0, "List VirtualList Table TreeView SelectionModel"},
        {"features/drawing", Category::Features, L"图片与绘图", L"图片缩放和原生 Canvas 图形。",
         PageKind::Drawing, 0, "ImageView Canvas Icon Sparkline"},
        {"features/notifications", Category::Features, L"弹层与通知",
         L"应用内反馈与系统通知分别体验。", PageKind::Notifications, 0,
         "Dialog Popup OverlayHost Toast StateView Badge StatusStrip"},
        {"features/menus", Category::Features, L"菜单与托盘", L"多级菜单、勾选、单选和快捷键。",
         PageKind::Menus, 0, "NativeMenu Menu Tray CommandScope"},
        {"features/windows", Category::Features, L"窗口行为", L"窗口状态、尺寸、全屏与关闭策略。",
         PageKind::Windows, 0, "Window WindowPlacement WindowTitleBar"},
        {"features/multiwindow", Category::Features, L"多窗口与跨窗拖放",
         L"独立窗口、父子模态及内容迁移。", PageKind::MultiWindow, 0, "Application DockSession"},
        {"features/files", Category::Features, L"剪贴板与文件交互",
         L"原生文件对话框、文本剪贴板与文件投递。", PageKind::Files, 0,
         "SystemClipboard NativeDialogs FileDrop"},
        {"features/terminal", Category::Features, L"终端与日志",
         L"四组模拟会话、文件操作、分栏、搜索与监控；不连接网络、不执行系统命令。", PageKind::Terminal, 0, "TerminalView LogView NativeHost Workspace"},
        {"features/remote", Category::Features, L"远程画面与输入",
         L"本地模拟帧和远程坐标、按键事件。", PageKind::Remote, 0,
         "RealtimeFrameView RemoteInputRegion"},
        {"features/reactive", Category::Features, L"状态绑定与异步任务",
         L"响应式状态、后台投递与取消。", PageKind::Reactive, 0,
         "State Computed Binding UiMailbox VmCommand NativeHost"},
        {"features/theme", Category::Features, L"主题与本地化", L"浅深主题、密度、语言和可读性。",
         PageKind::Theme, 0, "StyleSheet Density Localization"},
        {"features/diagnostics", Category::Features, L"无障碍与诊断",
         L"焦点、语义、实际布局与开发工具。", PageKind::Diagnostics, 0,
         "Accessibility FocusBookmark DeveloperTools LayoutSnapshot"},
    };
    return pages;
}
const PageDescriptor *findPage(const std::string &id) {
    const auto &pages = pageCatalog();
    auto found =
        std::find_if(pages.begin(), pages.end(), [&](const auto &page) { return id == page.id; });
    return found == pages.end() ? nullptr : &*found;
}
} // namespace lab

#include "page_factories.hpp"
#include <oneui/controls/nav_item.h>
#include <oneui/controls/status_strip.h>
#include <oneui/controls/window_title_bar.h>
#include <oneui/layout/app_shell.h>
#include <oneui/layout/top_bar.h>
#include <oneui/ui_native_theme.h>

namespace lab {
using namespace oneui;
namespace {
class ProductSurface final : public View {
    std::shared_ptr<AppShell> shell_;
    std::shared_ptr<TopBar> top_;
    std::shared_ptr<WindowTitleBar> title_;
    std::shared_ptr<Badge> badge_;

  public:
    ProductSurface(std::shared_ptr<AppShell> shell, std::shared_ptr<TopBar> top,
                   std::shared_ptr<WindowTitleBar> title, std::shared_ptr<Badge> badge)
        : shell_(std::move(shell)), top_(std::move(top)), title_(std::move(title)),
          badge_(std::move(badge)) {
        add(title_);
        add(shell_);
    }

  private:
    void layoutChildren() override {
        const auto rect = frame();
        const bool wide = rect.width >= 760;
        shell_->setSidebarVisible(wide);
        badge_->setVisible(wide);
        const float leading = std::max(100.f, rect.width - (wide ? 180.f + 310.f : 190.f));
        if (top_->leadingWidth() != leading)
            top_->setLeadingWidth(leading);
        title_->setFrame({rect.x, rect.y, rect.width, 34});
        shell_->setFrame({rect.x, rect.y + 34, rect.width, std::max(0.f, rect.height - 34)});
    }
};

class ProductPage final : public ContentPage {
    struct Lifetime {
        bool active = false;
    };
    std::shared_ptr<Lifetime> lifetime_ = std::make_shared<Lifetime>();
    std::shared_ptr<AppShell> shell_ = std::make_shared<AppShell>();
    std::shared_ptr<TopBar> top_ = std::make_shared<TopBar>();
    std::shared_ptr<WindowTitleBar> title_ =
        std::make_shared<WindowTitleBar>(L"本地连接助手 · 应用外壳预览");
    std::shared_ptr<Badge> badge_ = std::make_shared<Badge>(L"本地模拟", BadgeVariant::Success);
    std::shared_ptr<ProductSurface> root_;
    std::shared_ptr<Label> feedback_;
    std::shared_ptr<StatusStrip> status_ = std::make_shared<StatusStrip>(L"就绪", L"尚未执行操作");
    std::shared_ptr<TextField> partner_, code_, search_;
    std::shared_ptr<RadioGroup> mode_ = std::make_shared<RadioGroup>();
    std::shared_ptr<VirtualList> recents_ = std::make_shared<VirtualList>();
    std::shared_ptr<Select> navigation_ = std::make_shared<Select>();
    std::vector<std::shared_ptr<NavItem>> links_;
    unsigned codeVersion_ = 0;
    bool available_ = true;
    const std::vector<std::wstring> destinations_{L"远程协助", L"设备", L"工具箱", L"发现",
                                                  L"设置"};
    const std::vector<VirtualListItem> devices_{
        {L"办公室网关", L"169 510 1007", L"RDP", L"在线", {34, 197, 94}, {34, 197, 94}, true},
        {L"构建工作站", L"164 709 2397", L"SSH", L"忙碌", {220, 150, 30}, {220, 150, 30}, true}};

    template <class Callback> auto guarded(Callback callback) {
        return [weak = std::weak_ptr<Lifetime>(lifetime_),
                callback = std::move(callback)](auto... args) {
            if (auto state = weak.lock(); state && state->active)
                callback(args...);
        };
    }
    void report(std::wstring value) {
        feedback_->setText(value);
        status_->setMessage(std::move(value));
    }
    void navigate(int index) {
        if (index < 0 || index >= int(destinations_.size()))
            return;
        for (std::size_t i = 0; i < links_.size(); ++i)
            links_[i]->setSelected(int(i) == index);
        navigation_->setSelectedIndex(index);
        report(L"导航事件：" + destinations_[index] + L"；预览保留当前表单。");
    }
    void filter(const std::wstring &query) {
        std::vector<VirtualListItem> values;
        for (const auto &device : devices_)
            if (device.title.find(query) != std::wstring::npos ||
                device.detail.find(query) != std::wstring::npos)
                values.push_back(device);
        recents_->setRichItems(std::move(values));
        report(L"本地筛选：" + std::to_wstring(recents_->items().size()) + L" 台设备");
    }
    void buildChrome() {
        auto sidebar = std::make_shared<Stack>();
        sidebar->setEngine(StackEngine::Yoga);
        sidebar->setPadding(Insets{12});
        sidebar->setGap(8);
        const IconSymbol icons[] = {IconSymbol::RemoteAssist, IconSymbol::Device,
                                    IconSymbol::Toolbox, IconSymbol::Compass, IconSymbol::Settings};
        for (int i = 0; i < int(destinations_.size()); ++i) {
            auto item = std::make_shared<NavItem>(destinations_[i], icons[i]);
            item->setSelected(i == 0);
            item->setOnClick(guarded([this, i] { navigate(i); }));
            sidebar->add(item);
            sidebar->setFlex(item, {0, 0, 38, 38});
            links_.push_back(item);
        }
        shell_->setSidebar(sidebar);
        shell_->setSidebarWidth(180);
        shell_->setHeaderHeight(52);
        shell_->setFooterHeight(26);
        shell_->setFooterSpanSidebar(true);
        shell_->setContent(scroll_);
        search_ = std::make_shared<TextField>(L"按设备名称或 ID 筛选");
        search_->setPrefixIcon(IconSymbol::Search);
        search_->setAccessibleName(L"筛选本地模拟设备");
        search_->setOnChanged(guarded([this](const std::wstring &query) { filter(query); }));
        top_->setPadding(Insets{8});
        top_->setLeading(search_);
        badge_->setPreferredSize({92, 30});
        top_->addAction(badge_);
        auto density = std::make_shared<Tabs>();
        density->setItems({L"标准", L"紧凑"});
        density->setPreferredSize({132, 34});
        density->setOnChanged(guarded([this](int index) {
            recents_->setRowHeight(index ? 36 : 48);
            report(index ? L"列表密度：紧凑" : L"列表密度：标准");
        }));
        top_->addAction(density);
        auto bell = std::make_shared<IconButton>(IconSymbol::Bell);
        bell->setTooltip(L"查看本地通知事件");
        bell->setPreferredSize({30, 30});
        bell->setOnClick(
            guarded([this] { report(L"通知按钮已触发；系统通知请到“弹层与通知”页体验。"); }));
        top_->addAction(bell);
        shell_->setHeader(top_);
        auto footer = std::make_shared<Label>(L"本地模拟数据 · 不连接网络、不执行系统命令");
        footer->setFontSize(12);
        labels_.push_back(footer);
        shell_->setFooter(footer);
        title_->setOnMinimize(guarded([this] { report(L"预览标题栏事件：最小化"); }));
        title_->setOnMaximize(guarded([this] {
            title_->setMaximized(!title_->maximized());
            report(title_->maximized() ? L"预览标题栏事件：最大化" : L"预览标题栏事件：恢复");
        }));
        title_->setOnClose(
            guarded([this] { report(L"预览标题栏事件：关闭；实际窗口操作见“窗口行为”。"); }));
    }
    void buildForm() {
        text(L"体验完整的应用组合", 20);
        auto note = text(L"预览标题栏只显示事件；导航、搜索、表单与状态条共享本地状态。", 13);
        note->setTextWrapping(true);
        body_->setFlex(note, {0, 0, 44, 44});
        navigation_->setItems(destinations_);
        navigation_->setOnChanged(guarded([this](int index) { navigate(index); }));
        add(navigation_);
        feedback_ = text(L"选择设备或填写 ID，然后体验本地连接流程。");
        feedback_->setAccessibleName(L"组合页面事件结果");
        feedback_->setTextWrapping(true);
        body_->setFlex(feedback_, {0, 0, 60, 60});
        auto available = std::make_shared<Switch>(L"允许本地模拟协助");
        available->setChecked(true);
        available->setOnChanged(guarded([this](bool value) {
            available_ = value;
            report(value ? L"本地模拟协助已启用" : L"本地模拟协助已停用");
        }));
        add(available);
        text(L"本机 ID · 155 336 7617", 18);
        auto permanent = std::make_shared<TextField>(L"固定识别码");
        permanent->setText(L"DEMO-LOCAL");
        permanent->setReadOnly(true);
        permanent->setAccessibleName(L"只读的本地示例识别码");
        add(permanent);
        code_ = std::make_shared<TextField>(L"生成或编辑临时识别码");
        code_->setAccessibleName(L"临时识别码");
        add(code_);
        action(L"生成临时识别码", guarded([this] {
                   code_->setText(L"DEMO-" + std::to_wstring(++codeVersion_));
                   report(L"已生成本地示例识别码；不作为真实访问凭据。");
               }));
        text(L"协助其他设备", 18);
        partner_ = std::make_shared<TextField>(L"输入设备 ID，或选择下方设备");
        partner_->setPrefixIcon(IconSymbol::Device);
        partner_->setAccessibleName(L"对方设备 ID");
        add(partner_);
        mode_->setItems({L"远程桌面", L"远程文件"});
        mode_->setOrientation(RadioGroup::Orientation::Horizontal);
        add(mode_);
        action(L"模拟连接", guarded([this] {
                   if (!available_)
                       return report(L"本地模拟协助已停用，请先开启开关。");
                   if (partner_->text().empty())
                       return report(L"请填写设备 ID，或选择一台最近设备。");
                   report(L"本地连接请求：" + partner_->text() +
                          (mode_->selectedIndex() ? L" · 远程文件" : L" · 远程桌面"));
               }));
        text(L"最近设备", 16);
        recents_->setRichItems(devices_);
        recents_->setRowHeight(48);
        recents_->setOnChanged(guarded([this](int index) {
            if (index >= 0 && index < int(recents_->items().size())) {
                partner_->setText(recents_->items()[index].detail);
                report(L"已选择：" + recents_->items()[index].title);
            }
        }));
        add(recents_, 112);
        status_->setPrimaryAction(L"复制结果");
        status_->setSecondaryAction(L"详情");
        status_->setPrimaryActionPresentation(StatusStripActionPresentation::Link);
        status_->setOnPrimaryAction(guarded([this] {
            const auto value = feedback_->text();
            SystemClipboard clipboard;
            clipboard.setText(value);
            report(clipboard.text() == value ? L"事件结果已复制" : L"剪贴板当前不可用");
        }));
        status_->setOnSecondaryAction(guarded([this] {
            report(L"当前为本地模拟；识别码版本 " + std::to_wstring(codeVersion_) + L"，可见设备 " +
                   std::to_wstring(recents_->items().size()) + L" 台。");
        }));
        add(status_, 74);
    }

  public:
    ProductPage() {
        buildForm();
        buildChrome();
        root_ = std::make_shared<ProductSurface>(shell_, top_, title_, badge_);
    }
    ~ProductPage() override { lifetime_.reset(); }
    std::shared_ptr<Widget> widget() const override { return root_; }
    void activate() override { lifetime_->active = true; }
    void deactivate() override { lifetime_->active = false; }
    void setTheme(bool dark, bool compact) override {
        ContentPage::setTheme(dark, compact);
        styles_->addRulesFromCss(R"css(
          .nav-item { background:var(--surface); color:var(--ink); border-radius:5px; transition:0ms; }
          .nav-item:hover { background:var(--hover); }
          .nav-item.selected { background:var(--selection); color:var(--accent); }
          .status-strip { background:var(--surface); color:var(--ink); border-color:var(--line);
                          border-width:1px; padding:12px; }
          .status-strip-action { color:var(--accent); background:var(--surface);
                                 border-color:var(--line); border-width:1px; border-radius:5px; }
          .status-strip-action:hover { background:var(--hover); }
          .icon-button, .window-button { color:var(--ink); background:var(--surface); transition:0ms; }
          .icon-button:hover, .window-button:hover { background:var(--hover); }
          .nav-item:focus, .icon-button:focus, .status-strip-action:focus {
            border-color:var(--accent); border-width:1px; }
          .titlebar { background:var(--surface); color:var(--ink); }
          .titlebar-title { color:var(--ink); }
        )css");
        title_->setStyleSheet(styles_);
        for (auto &link : links_)
            link->setStyleSheet(styles_);
        status_->setStyleSheet(styles_, {"status-strip", {"status-strip"}, 0});
        std::function<void(Widget &)> apply = [&](Widget &widget) {
            ui::applyNativeTheme(widget, *styles_, false);
            if (auto button = dynamic_cast<IconButton *>(&widget))
                button->setStyleSheet(styles_, {"button", {"icon-button"}, 0});
            if (auto view = dynamic_cast<View *>(&widget))
                for (const auto &child : view->children())
                    apply(*child);
        };
        apply(*root_);
        StyleBox background;
        background.background.color = dark ? Color{23, 30, 33} : Color{255, 255, 255};
        shell_->setStyleBox(background);
        top_->setStyleBox(background);
    }
};
} // namespace
std::unique_ptr<DemoPage> createProductPage() { return std::make_unique<ProductPage>(); }
} // namespace lab

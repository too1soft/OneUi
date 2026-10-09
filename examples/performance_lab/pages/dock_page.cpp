#include "../terminal/bridge.hpp"
#include "page_factories.hpp"
#include <oneui/native_interop.h>
#include <sstream>

namespace lab {
using namespace oneui;
namespace {
class DockPage final : public ContentPage {
    struct Document {
        std::shared_ptr<TextField> editor;
    };
    Window &window_;
    DockSession session_;
    DockId mainHost_ = 0, selected_ = 1;
    std::shared_ptr<Label> status_;
    std::shared_ptr<Panel> surface_;
    std::shared_ptr<Menu> menu_ = std::make_shared<Menu>();
    std::shared_ptr<Button> menuButton_;
    std::vector<DockId> hidden_;
    void closeMenu() {
        menu_->setVisible(false);
        body_->setFlex(menu_, {0, 0, 0, 0});
    }
    void command(int id) {
        closeMenu();
        if (id == 0)
            report(session_.detachPane(selected_, WindowOptions{L"OneUI · 浮动面板", 640, 440}));
        else if (id == 1)
            report(session_.movePane(selected_, {mainHost_}));
        else if (id == 2) {
            DockId target = 0;
            for (auto pane : session_.panes(mainHost_))
                if (pane != selected_) {
                    target = pane;
                    break;
                }
            report(session_.movePane(selected_, {mainHost_, target, DockPosition::Right}));
        } else if (id == 3) {
            const auto current = session_.location(selected_).host;
            for (auto host : session_.hosts())
                if (host != current) {
                    report(session_.movePane(selected_, {host}));
                    return;
                }
            status_->setText(L"先拆出一个独立窗口，再选择需要迁移的面板。");
        } else if (id == 4) {
            auto before = session_.content(selected_);
            auto location = session_.location(selected_);
            auto token = session_.beginDrag(selected_);
            const bool cancelled = session_.cancelDrag(token);
            status_->setText(cancelled && before == session_.content(selected_) &&
                                     location.host == session_.location(selected_).host
                                 ? L"已取消：控件身份与宿主均未改变。"
                                 : L"取消失败，请查看诊断结果。");
        } else {
            const auto at = session_.location(selected_);
            const auto index = std::max(0, at.index + (id == 5 ? -1 : 1));
            report(session_.movePane(selected_, {at.host, selected_, DockPosition::Tab, index}));
        }
    }
    void report(DockSessionResult result) {
        if (result != DockSessionResult::Applied) {
            status_->setText(L"操作未完成，结果码：" + std::to_wstring(int(result)));
            return;
        }
        refresh();
    }
    void refresh() {
        auto location = session_.location(selected_);
        std::wostringstream message;
        message << L"当前面板 " << selected_ << L" · 宿主 " << location.host << L" · 标签位置 "
                << location.index + 1 << L" · 控件身份 " << session_.content(selected_).get()
                << L"\n系统窗口 " << session_.hosts().size()
                << L" 个；迁移保留编辑内容、选区与控制器。";
        status_->setText(message.str());
    }

  public:
    explicit DockPage(Window &window) : window_(window), session_(*Application::current()) {
        text(L"停靠与系统窗口 · 移动同一个控件", 18);
        text(L"先修改下方文档，再拆出或合并窗口；标签支持排序，分割线可拖动。", 14);
        auto selector = std::make_shared<Select>();
        selector->setItems({L"说明文档", L"工作草稿", L"配置文件", L"Rust 终端会话"});
        selector->setOnChanged([this](int index) {
            selected_ = DockId(index + 1);
            session_.selectPane(selected_);
            refresh();
        });
        add(selector);
        menuButton_ = action(L"面板操作菜单", [this] {
            if (menu_->visible()) {
                closeMenu();
                return;
            }
            menu_->setVisible(true);
            body_->setFlex(menu_, {0, 0, menu_->preferredHeight(), menu_->preferredHeight()});
            window_.requestFocus(menu_.get());
        });
        for (const auto title : {L"拆出到独立窗口", L"合并回主工作区", L"右侧分栏", L"移到另一窗口",
                                 L"取消迁移", L"标签向前", L"标签向后"})
            menu_->addItem(title);
        menu_->setOnItemActivated([this](int index) { command(index); });
        menu_->setVisible(false);
        add(menu_, 0);
        auto surface = surface_ = std::make_shared<Panel>();
        surface->setBorderWidth(0);
        mainHost_ = session_.attachHost(window_, surface);
        for (DockId id = 1; id <= 3; ++id) {
            auto doc = std::make_shared<Document>();
            doc->editor = std::make_shared<TextField>();
            doc->editor->setMultiline(true);
            doc->editor->setClipboard(std::make_shared<SystemClipboard>());
            doc->editor->setText(id == 1 ? L"这是同一个编辑器。\n\n选中一段文字，再拆出到独立系统窗"
                                           L"口；返回时内容与选区仍然保留。"
                                 : id == 2 ? L"在这里写下草稿…"
                                           : L"theme = green\nworkspace = docked");
            session_.addPane({id,
                              id == 1   ? L"说明"
                              : id == 2 ? L"草稿"
                                        : L"配置",
                              doc->editor, doc},
                             {mainHost_});
        }
        auto owner = std::shared_ptr<void>(terminal_demo_create_v1(), terminal_demo_destroy_v1);
        if (!owner)
            throw std::runtime_error("Cannot create Rust dock terminal owner");
        auto terminal = retainNativeWidget(terminal_demo_widget_v1(owner.get(), 0));
        if (!terminal || session_.addPane({4, L"Rust 终端", terminal, owner}, {mainHost_}) !=
                             DockSessionResult::Applied)
            throw std::runtime_error("Cannot attach Rust dock terminal");
        add(surface, 340);
        session_.selectPane(selected_);
        status_ = text(L"", 13);
        text(session_.nativeDragSupported(mainHost_)
                 ? (window_.backend() == WindowBackend::Wayland
                        ? L"可将标签拖入另一窗口。此环境不支持连续拖出顶层窗口，请使用下方拆出和合"
                          L"并操作。"
                        : L"可将标签拖到另一窗口，或拖出窗口后释放以创建独立窗口。")
                 : L"当前后端尚未启用原生标签拖放；下方拆分、合并和取消操作可用。",
             13);
        status_->setTextWrapping(true);
        body_->setFlex(status_, {0, 0, 72, 72});
        action(L"拆出到独立窗口", [this] { command(0); });
        action(L"合并回主工作区", [this] { command(1); });
        action(L"在主工作区右侧分栏", [this] { command(2); });
        action(L"移到另一窗口", [this] { command(3); });
        action(L"取消一次迁移事务", [this] { command(4); });
        session_.setOnChanged([this] { refresh(); });
        refresh();
    }
    ~DockPage() override { session_.setOnChanged({}); }
    void setTheme(bool dark, bool compact) override {
        ContentPage::setTheme(dark, compact);
        session_.setStyleSheet(styles_);
        menu_->setStyleSheet(styles_, {"menu", {}, 0});
    }
    bool key(const KeyEvent &event) override {
        if (event.key == Key::Escape && menu_->visible()) {
            closeMenu();
            window_.requestFocus(menuButton_.get());
            return true;
        }
        return false;
    }
    void deactivate() override {
        closeMenu();
        surface_->setVisible(false);
        hidden_.clear();
        for (auto host : session_.hosts())
            if (host != mainHost_) {
                if (auto window = session_.hostWindow(host); window && window->state().visible) {
                    if (window->hide())
                        hidden_.push_back(host);
                }
            }
    }
    void activate() override {
        surface_->setVisible(true);
        for (auto host : hidden_)
            if (auto window = session_.hostWindow(host))
                window->show();
        hidden_.clear();
    }
};
} // namespace
std::unique_ptr<DemoPage> createDockPage(Window &window) {
    return std::make_unique<DockPage>(window);
}
} // namespace lab

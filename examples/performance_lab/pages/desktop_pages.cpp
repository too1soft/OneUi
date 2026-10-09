#include "page_factories.hpp"
#include <oneui/controls/dialog.h>
#include <oneui/controls/toast_host.h>

namespace lab {
using namespace oneui;
namespace {
class MenusPage final : public ContentPage {
    Window &window_;
    bool checked_ = false, enabled_ = false;
    int selectedProfile_ = 0;
    std::shared_ptr<Label> result_;
    void menu() {
        NativeMenuItem toggle{201, L"演示勾选"};
        toggle.check = MenuCheck::Check;
        toggle.checked = checked_;
        NativeMenuItem submenu{0, L"子菜单"};
        submenu.children = {{202, L"恢复窗口"}, {203, L"禁用项目", false}};
        NativeMenuItem first{205, L"配置一"}, second{206, L"配置二"};
        first.check = second.check = MenuCheck::Radio;
        first.radioGroup = second.radioGroup = 1;
        first.checked = selectedProfile_ == 0;
        second.checked = selectedProfile_ == 1;
        if (!window_.setTrayMenu({toggle,
                                  submenu,
                                  first,
                                  second,
                                  {0, {}, true, true},
                                  {204, L"显示试验台"},
                                  {207, L"退出应用"}}))
            result_->setText(L"托盘菜单未被当前环境接受。");
    }

  public:
    explicit MenusPage(Window &window) : window_(window) {
        text(L"应用菜单栏与导航共享页面入口；托盘菜单独立。", 18);
        result_ = text(L"点击启用托盘，再从系统托盘操作。");
        action(L"启用托盘", [this] {
            menu();
            window_.setTrayTooltip(L"OneUI 试验台");
            enabled_ = window_.setTrayEnabled(true);
            result_->setText(enabled_ ? L"托盘请求已接受；可体验勾选、单选和子菜单。"
                                      : L"当前环境无法创建托盘");
        });
        action(L"隐藏到托盘", [this] {
            if (!enabled_) {
                result_->setText(L"请先启用托盘，确保有可用的恢复入口。");
                return;
            }
            if (!window_.hide())
                result_->setText(L"当前环境未接受隐藏请求。");
        });
        action(L"启用关闭到托盘", [this] {
            if (enabled_) {
                window_.setCloseToTray(true);
                result_->setText(L"已启用；关闭按钮会隐藏窗口，可从托盘恢复或退出。");
            } else
                result_->setText(L"请先启用托盘。");
        });
        action(L"移除托盘", [this] {
            window_.setCloseToTray(false);
            window_.setTrayEnabled(false);
            enabled_ = false;
            result_->setText(L"托盘已移除");
        });
    }
    void activate() override {
        window_.setOnTrayAction([this](int id) {
            if (id == 201) {
                checked_ = !checked_;
                menu();
            }
            if (id == 202 || id == 204) {
                window_.show();
                window_.activate();
            }
            if (id == 205 || id == 206) {
                selectedProfile_ = id - 205;
                menu();
            }
            if (id == 207)
                Application::current()->requestQuit();
            result_->setText(L"托盘菜单 ID：" + std::to_wstring(id));
        });
    }
    void deactivate() override {
        window_.setOnTrayAction({});
        window_.setCloseToTray(false);
        window_.setTrayEnabled(false);
        enabled_ = false;
    }
};
class WindowPage final : public ContentPage {
    Window &window_;
    Window::EventSubscription stateSubscription_;
    WindowPlacement placement_{};
    bool saved_ = false, topmost_ = false, resizable_ = true;
    std::shared_ptr<Label> result_, requested_;
    void describe(const WindowState &state) {
        auto known = [&](WindowStateField field, bool value) -> std::wstring {
            return !(state.knownFields & std::uint32_t(field)) ? L"未知" : value ? L"是" : L"否";
        };
        result_->setText(L"系统确认 · 可见：" + known(WindowStateField::Visible, state.visible) +
                         L"，最小化：" + known(WindowStateField::Minimized, state.minimized) +
                         L"，最大化：" + known(WindowStateField::Maximized, state.maximized) +
                         L"\n全屏：" + known(WindowStateField::Fullscreen, state.fullscreen) +
                         L"，置顶：" + known(WindowStateField::Topmost, state.topmost) +
                         L"，可调整：" + known(WindowStateField::Resizable, state.resizable) +
                         L"\n客户区 " + std::to_wstring(int(state.clientSize.width)) + L" × " +
                         std::to_wstring(int(state.clientSize.height)) + L"；DPI " +
                         std::to_wstring(state.dpiScale));
    }

  public:
    explicit WindowPage(Window &window) : window_(window) {
        result_ = text(L"窗口状态");
        result_->setTextWrapping(true);
        body_->setFlex(result_, {0, 0, 96, 96});
        requested_ = text(L"请求状态与系统确认的实际状态分别显示。");
        action(L"最小化", [this] {
            requested_->setText(L"已提交最小化请求。");
            window_.minimize();
        });
        action(L"最大化", [this] {
            requested_->setText(window_.maximize() ? L"最大化请求已接受。"
                                                   : L"当前后端未接受最大化请求。");
        });
        action(L"恢复", [this] {
            requested_->setText(window_.restore() ? L"恢复请求已接受。" : L"恢复请求未接受。");
        });
        action(L"切换全屏", [this] {
            requested_->setText(L"已提交全屏切换请求。");
            window_.setFullscreen(!window_.state().fullscreen);
        });
        action(L"切换置顶", [this] {
            if (window_.setTopmost(!topmost_)) {
                topmost_ = !topmost_;
                requested_->setText(topmost_ ? L"已请求置顶。" : L"已请求取消置顶。");
            } else
                requested_->setText(L"当前后端未接受置顶请求。");
        });
        action(L"切换可调整尺寸", [this] {
            if (window_.setResizable(!resizable_)) {
                resizable_ = !resizable_;
                requested_->setText(resizable_ ? L"已请求允许调整尺寸。" : L"已请求锁定尺寸。");
            } else
                requested_->setText(L"当前后端未接受调整尺寸策略。");
        });
        action(L"保存窗口位置", [this] {
            saved_ = window_.getWindowPlacement(placement_);
            requested_->setText(saved_ ? L"位置已保存在内存" : L"当前后端不提供绝对位置");
        });
        action(L"恢复已保存位置", [this] {
            requested_->setText(saved_ && window_.setWindowPlacement(placement_)
                                    ? L"恢复请求已接受"
                                    : L"没有可恢复的位置或当前环境不支持");
        });
        action(L"设置窗口为 900 × 720", [this] {
            requested_->setText(window_.setClientSize({900, 720}) ? L"尺寸请求已接受。"
                                                                  : L"尺寸请求未接受。");
        });
        action(L"限制最大尺寸为 1440 × 960", [this] {
            requested_->setText(window_.setMaximumClientSize({1440, 960})
                                    ? L"最大尺寸约束请求已接受。"
                                    : L"当前后端未接受尺寸约束。");
        });
    }
    void activate() override {
        describe(window_.state());
        stateSubscription_ =
            window_.observeState([this](const WindowState &state) { describe(state); });
    }
    void deactivate() override { stateSubscription_.reset(); }
};
} // namespace
std::unique_ptr<DemoPage> createDesktopPage(const PageDescriptor &page, Window &window) {
    if (page.kind == PageKind::Menus)
        return std::make_unique<MenusPage>(window);
    if (page.kind == PageKind::Windows)
        return std::make_unique<WindowPage>(window);
    return createNotificationsPage();
}
} // namespace lab

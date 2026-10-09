#include "page_factories.hpp"
#include <oneui/modal_session.h>
#include <oneui/ui_native_theme.h>

namespace lab {
namespace {
using namespace oneui;
struct ObserverWindow {
    std::unique_ptr<Window> window;
    std::shared_ptr<Stack> body;
    std::shared_ptr<Label> result;
    std::shared_ptr<Button> accept, cancel;
    std::function<void(bool)> closeDecision;
    bool confirmClose = true;
    unsigned commands = 0;
    void answer(bool acceptClose) {
        auto decision = std::move(closeDecision);
        accept->setVisible(false);
        cancel->setVisible(false);
        result->setText(acceptClose ? L"关闭已确认" : L"已取消关闭，窗口继续使用。");
        if (decision)
            decision(acceptClose);
    }
    ~ObserverWindow() {
        if (closeDecision)
            closeDecision(false);
        if (window) {
            window->setCloseToTray(false);
            window->close();
        }
    }
};
struct ModalWindow {
    std::unique_ptr<Window> window;
    std::shared_ptr<Stack> body;
    std::unique_ptr<ModalSession> session;
    ~ModalWindow() {
        session.reset();
        if (window)
            window->close();
    }
};
class MultiWindowPage final : public ContentPage {
    Window &parent_;
    Application &app_;
    State<std::wstring> sharedText_{L"窗口间共享的类型化状态"};
    std::vector<std::shared_ptr<ObserverWindow>> observers_;
    std::shared_ptr<ModalWindow> modal_;
    std::shared_ptr<Label> status_;
    std::shared_ptr<Select> policy_;
    bool resident_ = false, dark_ = true;
    std::shared_ptr<Stack> windowBody() {
        auto body = std::make_shared<Stack>();
        body->setEngine(StackEngine::Yoga);
        body->setPadding(Insets{24});
        body->setGap(12);
        return body;
    }
    void decorate(Widget &widget) {
        widget.setStyleScope(styles_);
        ui::applyNativeTheme(widget, *styles_, false);
        if (auto label = dynamic_cast<Label *>(&widget))
            label->setColor(dark_ ? Color{229, 236, 232} : Color{28, 43, 35});
        if (auto button = dynamic_cast<Button *>(&widget))
            applyDemoButton(*button, *styles_);
        if (auto stack = dynamic_cast<Stack *>(&widget)) {
            StyleBox box;
            box.background.color = dark_ ? Color{23, 30, 33} : Color{255, 255, 255};
            stack->setStyleBox(box);
        }
        if (auto view = dynamic_cast<View *>(&widget))
            for (auto &child : view->children())
                decorate(*child);
    }
    void observer() {
        observers_.erase(std::remove_if(observers_.begin(), observers_.end(),
                                        [](auto &item) { return item->window->closed(); }),
                         observers_.end());
        if (observers_.size() >= 8) {
            status_->setText(L"最多同时打开 8 个观察窗口。");
            return;
        }
        auto child = std::make_shared<ObserverWindow>();
        child->window = Window::create(L"OneUI · 观察窗口", 540, 380);
        child->body = windowBody();
        auto value = std::make_shared<Label>();
        value->setFontSize(20);
        value->bindText(sharedText_);
        child->body->add(value);
        child->body->setFlex(value, {0, 0, 64, 64});
        child->result = std::make_shared<Label>(L"关闭窗口可体验异步确认。");
        child->result->setTextWrapping(true);
        child->body->add(child->result);
        child->body->setFlex(child->result, {0, 0, 60, 60});
        std::weak_ptr<ObserverWindow> weak = child;
        auto confirm = std::make_shared<Checkbox>(L"关闭前确认");
        confirm->setChecked(true);
        confirm->setOnChanged([weak](bool value) {
            if (auto live = weak.lock())
                live->confirmClose = value;
        });
        child->body->add(confirm);
        child->accept = std::make_shared<Button>(L"允许关闭");
        child->cancel = std::make_shared<Button>(L"保留窗口");
        child->accept->setOnClick([weak] {
            if (auto live = weak.lock())
                live->answer(true);
        });
        child->cancel->setOnClick([weak] {
            if (auto live = weak.lock())
                live->answer(false);
        });
        for (auto button : {child->accept, child->cancel}) {
            child->body->add(button);
            child->body->setFlex(button, {0, 0, 40, 40});
            button->setVisible(false);
        }
        decorate(*child->body);
        child->window->setContent(child->body);
        if (!app_.addWindow(*child->window)) {
            status_->setText(L"应用正在退出，未创建观察窗口。");
            return;
        }
        app_.setCloseGuard(*child->window, [weak](std::function<void(bool)> answer) {
            auto live = weak.lock();
            if (!live || !live->confirmClose) {
                answer(true);
                return;
            }
            if (live->closeDecision) {
                answer(false);
                return;
            }
            live->closeDecision = std::move(answer);
            live->result->setText(L"收到关闭请求。选择允许或取消；事件循环仍在运行。");
            live->accept->setVisible(true);
            live->cancel->setVisible(true);
            live->window->show();
            live->window->activate();
        });
        child->window->show();
        child->window->activate();
        observers_.push_back(std::move(child));
        status_->setText(L"已创建观察窗口；编辑上方文字会同步更新。");
    }
    void modal() {
        if (modal_ && !modal_->window->closed()) {
            modal_->window->activate();
            return;
        }
        auto dialog = std::make_shared<ModalWindow>();
        dialog->window = Window::create(L"OneUI · 异步父子模态", 500, 260);
        dialog->body = windowBody();
        auto hint = std::make_shared<Label>(L"父窗口输入暂时受限；没有嵌套事件循环。");
        hint->setTextWrapping(true);
        dialog->body->add(hint);
        dialog->body->setFlex(hint, {0, 0, 72, 72});
        std::weak_ptr<ModalWindow> weak = dialog;
        for (bool accept : {true, false}) {
            auto button = std::make_shared<Button>(accept ? L"确认并返回 1" : L"取消并返回 0");
            button->setOnClick([weak, accept] {
                if (auto live = weak.lock(); live && live->session)
                    live->session->finish(accept ? 1 : 0);
            });
            dialog->body->add(button);
            dialog->body->setFlex(button, {0, 0, 40, 40});
        }
        decorate(*dialog->body);
        dialog->window->setContent(dialog->body);
        if (!app_.addWindow(*dialog->window))
            return;
        dialog->session = ModalSession::show(
            *dialog->window, parent_, [result = std::weak_ptr<Label>(status_)](int code) {
                if (auto label = result.lock())
                    label->setText(L"模态窗口已完成，结果 " + std::to_wstring(code) +
                                   L"；父窗口恢复输入。");
            });
        if (!dialog->session) {
            status_->setText(L"当前环境未接受父子模态请求。");
            return;
        }
        modal_ = std::move(dialog);
    }
    void policy(int index) {
        if (index == 2 && !resident_) {
            parent_.setTrayMenu({{9501, L"显示试验台"}, {9502, L"退出应用"}});
            if (!parent_.setTrayEnabled(true)) {
                policy_->setSelectedIndex(0);
                status_->setText(L"当前环境无法创建托盘，保持“最后窗口关闭退出”。");
                app_.setQuitPolicy(QuitPolicy::LastWindowClosed);
                return;
            }
            resident_ = true;
            parent_.setCloseToTray(true);
        } else if (index != 2 && resident_) {
            parent_.setCloseToTray(false);
            parent_.setTrayEnabled(false);
            resident_ = false;
        }
        app_.setQuitPolicy(QuitPolicy(index));
        status_->setText(index == 2
                             ? L"显式退出已启用：关闭主窗口会隐藏到托盘，可从托盘恢复或退出。"
                             : L"退出策略已更新。退出应用会先询问所有观察窗口。");
    }

  public:
    explicit MultiWindowPage(Window &parent) : parent_(parent), app_(*Application::current()) {
        text(L"多窗口 · 同一个 UI 线程与应用生命周期", 18);
        auto input = std::make_shared<TextField>();
        input->setText(sharedText_.get());
        input->setOnChanged([this](const std::wstring &value) { sharedText_.set(value); });
        add(input);
        status_ = text(L"观察窗口共享文字；关闭时可以取消，或稍后再确认。");
        status_->setTextWrapping(true);
        body_->setFlex(status_, {0, 0, 64, 64});
        action(L"创建观察窗口", [this] { observer(); });
        action(L"请求关闭所有观察窗口", [this] {
            for (auto &child : observers_)
                if (!child->window->closed())
                    child->window->requestClose();
        });
        action(L"向观察窗口投递命令", [this] {
            enum class Command { MarkReceived };
            for (auto &child : observers_) {
                std::weak_ptr<ObserverWindow> weak = child;
                app_.post([weak, command = Command::MarkReceived] {
                    auto live = weak.lock();
                    if (!live || live->window->closed())
                        return;
                    if (command == Command::MarkReceived)
                        live->result->setText(L"收到 UI dispatcher 命令 " +
                                              std::to_wstring(++live->commands));
                });
            }
        });
        action(L"打开异步父子模态窗口", [this] { modal(); });
        text(L"标签拆出、合并与跨窗拖放见“布局 / 停靠与浮动”。");
        policy_ = std::make_shared<Select>();
        policy_->setItems({L"最后窗口关闭退出", L"主窗口关闭退出", L"显式退出 · 托盘驻留"});
        policy_->setOnChanged([this](int index) {
            if (index >= 0 && index < 3)
                policy(index);
        });
        add(policy_);
        action(L"请求退出整个试验台", [this] { app_.requestQuit(); });
    }
    void setTheme(bool dark, bool compact) override {
        dark_ = dark;
        ContentPage::setTheme(dark, compact);
        for (auto &child : observers_)
            decorate(*child->body);
        if (modal_)
            decorate(*modal_->body);
    }
    void activate() override {
        parent_.setOnTrayAction([this](int command) {
            if (command == 9501) {
                parent_.show();
                parent_.activate();
            }
            if (command == 9502)
                app_.requestQuit();
        });
    }
    void deactivate() override {
        if (resident_)
            policy(0);
        policy_->setSelectedIndex(0);
        app_.setQuitPolicy(QuitPolicy::LastWindowClosed);
        parent_.setOnTrayAction({});
    }
    ~MultiWindowPage() override {
        deactivate();
        modal_.reset();
        for (auto &child : observers_) {
            // Closed windows have already retired their application entry.
            if (!child->window->closed())
                app_.setCloseGuard(*child->window, {});
            child->answer(false);
        }
        observers_.clear();
    }
};
} // namespace
std::unique_ptr<DemoPage> createMultiWindowPage(Window &window) {
    return std::make_unique<MultiWindowPage>(window);
}
} // namespace lab

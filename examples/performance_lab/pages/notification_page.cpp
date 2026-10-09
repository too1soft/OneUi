#include "../popup_showcase.hpp"
#include "internal/unicode.h"
#include "page_factories.hpp"
#include <oneui/controls/dialog.h>
#include <oneui/controls/toast_host.h>
#include <oneui/notification.h>

namespace lab {
using namespace oneui;
namespace {
std::wstring statusName(NotificationStatus value) {
    switch (value) {
    case NotificationStatus::Accepted:
        return L"请求已接受";
    case NotificationStatus::PermissionDenied:
        return L"权限拒绝";
    case NotificationStatus::ServiceUnavailable:
        return L"服务不可用";
    case NotificationStatus::Unsupported:
        return L"不支持";
    default:
        return L"失败";
    }
}
std::wstring permissionName(NotificationPermission value) {
    switch (value) {
    case NotificationPermission::Granted:
        return L"已允许";
    case NotificationPermission::Denied:
        return L"已拒绝";
    case NotificationPermission::NotDetermined:
        return L"尚未决定";
    case NotificationPermission::Unsupported:
        return L"不支持";
    default:
        return L"系统未提供状态";
    }
}
class NotificationsPage final : public ContentPage {
    std::shared_ptr<OverlayHost> root_ = std::make_shared<OverlayHost>();
    std::shared_ptr<ToastHost> toasts_ = std::make_shared<ToastHost>();
    std::shared_ptr<Label> systemResult_;
    std::unique_ptr<NotificationService> system_;
    int sequence_ = 0;
    struct Life {
        bool active = false;
    };
    std::shared_ptr<Life> life_ = std::make_shared<Life>();
    std::shared_ptr<PopupShowcase> popups_ = std::make_shared<PopupShowcase>();
    NotificationService::Completion completion() {
        std::weak_ptr<Label> weak = systemResult_;
        return [weak, life = std::weak_ptr<Life>(life_)](const NotificationResult &result) {
            auto live = life.lock();
            if (!live || !live->active)
                return;
            if (auto label = weak.lock())
                label->setText(L"系统通知：" + statusName(result.status) + L"\n" +
                               oneui::unicode::fromUtf8(result.detail));
        };
    }
    void systemControls() {
        text(L"系统通知 · 与托盘、Toast 独立", 18);
        text(L"系统设置和专注模式决定是否显示。“请求已接受”不表示用户已经看到。", 14);
        systemResult_ = text(L"尚未查询权限");
        systemResult_->setTextWrapping(true);
        systemResult_->setMaxLines(3);
        body_->setFlex(systemResult_, StackFlex{0, 0, 76, 76});
        auto life = std::weak_ptr<Life>(life_);
        auto query = [this, life](bool request) {
            auto live = life.lock();
            if (!live || !live->active || !system_)
                return;
            std::weak_ptr<Label> weak = systemResult_;
            auto result = [weak, life](const NotificationPermissionResult &value) {
                auto live = life.lock();
                if (!live || !live->active)
                    return;
                if (auto label = weak.lock())
                    label->setText(permissionName(value.permission) + L" · " +
                                   statusName(value.status) + L"\n" +
                                   oneui::unicode::fromUtf8(value.detail));
            };
            if (request)
                system_->requestPermission(result);
            else
                system_->queryPermission(result);
        };
        action(L"查询系统通知权限", [query] { query(false); });
        action(L"请求系统通知权限", [query] { query(true); });
        action(L"发送 / 更新同一条系统通知", [this, life] {
            auto live = life.lock();
            if (!live || !live->active || !system_)
                return;
            Notification value;
            value.id = "oneui-lab-task";
            value.title = L"OneUI 试验台";
            value.body = L"本地通知版本 " + std::to_wstring(++sequence_);
            value.parameter = "features/notifications";
            value.actions = {{"open", L"查看结果"}, {"acknowledge", L"确认"}};
            system_->send(std::move(value), completion());
        });
        action(L"撤销系统通知", [this, life] {
            auto live = life.lock();
            if (live && live->active && system_)
                system_->withdraw("oneui-lab-task", completion());
        });
    }

  public:
    NotificationsPage() {
        root_->setContent(scroll_);
        text(L"弹层 · 交互策略、边界翻转与多层焦点", 18);
        add(popups_, 440);
        systemControls();
        text(L"应用内 Toast · 队列、同 ID 更新与关闭", 18);
        auto result = text(L"暂无应用内通知操作");
        auto life = std::weak_ptr<Life>(life_);
        auto send = [this, result, life](std::string id) {
            auto live = life.lock();
            if (!live || !live->active)
                return;
            ToastMessage message;
            message.id = std::move(id);
            message.title = L"本地任务";
            message.message = L"消息版本 " + std::to_wstring(++sequence_);
            message.primaryAction = L"查看";
            message.secondaryAction = L"忽略";
            message.onAction = [result, life](const std::string &action) {
                auto live = life.lock();
                if (!live || !live->active)
                    return;
                result->setText(L"Toast 回调：" + oneui::unicode::fromUtf8(action));
            };
            toasts_->push(std::move(message));
            result->setText(L"队列数量：" + std::to_wstring(toasts_->pendingCount()));
        };
        action(L"发送 / 更新同一条 Toast", [send] { send("task"); });
        action(L"追加 Toast", [this, send, life] {
            auto live = life.lock();
            if (live && live->active)
                send("message-" + std::to_string(sequence_));
        });
        action(L"撤销 task", [this, life] {
            auto live = life.lock();
            if (live && live->active)
                toasts_->dismiss("task");
        });
        add(toasts_, 160);
    }
    ~NotificationsPage() override {
        deactivate();
        life_.reset();
    }
    void setTheme(bool dark, bool compact) override {
        ContentPage::setTheme(dark, compact);
        popups_->setTheme(styles_, dark);
    }
    std::shared_ptr<Widget> widget() const override { return root_; }
    void activate() override {
        life_->active = true;
        popups_->setActive(true);
        toasts_->setActive(true);
        if (auto *app = Application::current())
            system_ = std::make_unique<NotificationService>(*app);
    }
    void deactivate() override {
        life_->active = false;
        popups_->setActive(false);
        root_->clearOverlays();
        toasts_->setActive(false);
        system_.reset();
    }
    void applicationActivated(const ApplicationActivation &value) override {
        systemResult_->setText(
            L"系统唤起 · 通知 " + oneui::unicode::fromUtf8(value.notificationId) + L" · 操作 " +
            oneui::unicode::fromUtf8(value.actionId.empty() ? "default" : value.actionId) +
            L"\n应用参数：" + oneui::unicode::fromUtf8(value.parameter));
    }
};
} // namespace
std::unique_ptr<DemoPage> createNotificationsPage() {
    return std::make_unique<NotificationsPage>();
}
} // namespace lab

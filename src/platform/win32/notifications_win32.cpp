#include "internal/unicode.h"
#include "notification_activation_win32.h"
#include "notification_runtime_win32.h"
#include "platform/shared/notification_backend.h"
#include <windows.data.xml.dom.h>
#include <windows.ui.notifications.h>

namespace oneui::platform {
namespace {
using namespace win32;
using namespace ABI::Windows::UI::Notifications;
using namespace ABI::Windows::Data::Xml::Dom;
std::wstring xml(const std::wstring &input) {
    std::wstring result;
    for (wchar_t c : input) {
        switch (c) {
        case L'&':
            result += L"&amp;";
            break;
        case L'<':
            result += L"&lt;";
            break;
        case L'>':
            result += L"&gt;";
            break;
        case L'"':
            result += L"&quot;";
            break;
        case L'\'':
            result += L"&apos;";
            break;
        default:
            if (c < 32 && c != L'\t' && c != L'\n' && c != L'\r')
                result += L' ';
            else
                result += c;
        }
    }
    return result;
}
std::wstring payload(const Notification &value, const std::string &action = {}) {
    return unicode::fromUtf8(encodeActivation({value.id, action, value.parameter}));
}
class WindowsNotifications final : public NotificationBackend {
    std::string id_;
    NotificationActivation activation_;
    ComPtr<IToastNotifier> notifier_;
    NotificationStatus failureStatus_ = NotificationStatus::ServiceUnavailable;
    std::string failure_;
    bool ready() {
        if (!activation_.primary()) {
            failure_ = "A primary application instance already owns notifications";
            return false;
        }
        if (!activation_.registered()) {
            failure_ = "Notification COM activation registration failed";
            return false;
        }
        if (!NotificationRuntime::get().available()) {
            failureStatus_ = NotificationStatus::Unsupported;
            failure_ = "Native interactive notifications require Windows 10 or later";
            return false;
        }
        if (!notificationRegistrationExists(id_)) {
            failure_ =
                "Install the application's Start Menu shortcut and COM activation registration";
            return false;
        }
        try {
            if (!notifier_) {
                auto manager = notificationFactory<IToastNotificationManagerStatics>(
                    RuntimeClass_Windows_UI_Notifications_ToastNotificationManager);
                // History2 is required for stable-ID withdrawal on Windows 10.
                auto modern = notificationFactory<IToastNotificationManagerStatics2>(
                    RuntimeClass_Windows_UI_Notifications_ToastNotificationManager);
                (void)modern;
                checkNotificationHr(manager->CreateToastNotifierWithId(
                    NotificationString(unicode::fromUtf8(id_)), &notifier_));
            }
            return true;
        } catch (const std::exception &error) {
            failure_ = error.what();
            return false;
        }
    }

  public:
    WindowsNotifications(const ApplicationOptions &options, Activation activation)
        : id_(options.id), activation_(options, std::move(activation)) {}
    bool primary() const override { return activation_.primary(); }
    void permission(bool, Permission callback) override {
        if (!ready()) {
            callback({failureStatus_ == NotificationStatus::Unsupported
                          ? NotificationPermission::Unsupported
                          : NotificationPermission::Unknown,
                      failureStatus_, failure_});
            return;
        }
        NotificationSetting setting{};
        const auto result = notifier_->get_Setting(&setting);
        if (FAILED(result)) {
            callback({NotificationPermission::Unknown, NotificationStatus::Failed,
                      "Unable to query Windows notification settings"});
            return;
        }
        const bool allowed = setting == NotificationSetting_Enabled;
        callback({allowed ? NotificationPermission::Granted : NotificationPermission::Denied,
                  allowed ? NotificationStatus::Accepted : NotificationStatus::PermissionDenied,
                  allowed ? "Windows allows notification requests"
                          : "Notifications are disabled in Windows Settings or by policy; Windows "
                            "exposes no permission prompt"});
    }
    void send(const Notification &value, Result callback) override {
        if (!ready()) {
            callback({failureStatus_, value.id, failure_});
            return;
        }
        try {
            NotificationSetting setting{};
            checkNotificationHr(notifier_->get_Setting(&setting));
            if (setting != NotificationSetting_Enabled) {
                callback({NotificationStatus::PermissionDenied, value.id,
                          "Windows has disabled notifications"});
                return;
            }
            std::wstring document =
                L"<toast activationType=\"foreground\" launch=\"" + payload(value) +
                L"\"><visual><binding template=\"ToastGeneric\"><text>" + xml(value.title) +
                L"</text><text>" + xml(value.body) + L"</text></binding></visual>";
            if (!value.actions.empty()) {
                document += L"<actions>";
                for (const auto &action : value.actions)
                    document += L"<action activationType=\"foreground\" content=\"" +
                                xml(action.title) + L"\" arguments=\"" + payload(value, action.id) +
                                L"\"/>";
                document += L"</actions>";
            }
            document += L"</toast>";
            ComPtr<IInspectable> object;
            checkNotificationHr(NotificationRuntime::get().activate(
                NotificationString(RuntimeClass_Windows_Data_Xml_Dom_XmlDocument), &object));
            ComPtr<IXmlDocument> dom;
            ComPtr<IXmlDocumentIO> io;
            checkNotificationHr(object.As(&dom));
            checkNotificationHr(object.As(&io));
            checkNotificationHr(io->LoadXml(NotificationString(document)));
            auto factory = notificationFactory<IToastNotificationFactory>(
                RuntimeClass_Windows_UI_Notifications_ToastNotification);
            ComPtr<IToastNotification> toast;
            checkNotificationHr(factory->CreateToastNotification(dom.Get(), &toast));
            ComPtr<IToastNotification2> tagged;
            checkNotificationHr(toast.As(&tagged));
            checkNotificationHr(tagged->put_Tag(NotificationString(notificationTag(value.id))));
            checkNotificationHr(tagged->put_Group(NotificationString(L"oneui")));
            checkNotificationHr(notifier_->Show(toast.Get()));
            callback(
                {NotificationStatus::Accepted, value.id,
                 "Request accepted by Windows; Focus Assist and user settings control display"});
        } catch (const std::exception &error) {
            callback({NotificationStatus::Failed, value.id, error.what()});
        }
    }
    void withdraw(const std::string &id, Result callback) override {
        if (!ready()) {
            callback({failureStatus_, id, failure_});
            return;
        }
        try {
            auto manager = notificationFactory<IToastNotificationManagerStatics2>(
                RuntimeClass_Windows_UI_Notifications_ToastNotificationManager);
            ComPtr<IToastNotificationHistory> history;
            checkNotificationHr(manager->get_History(&history));
            checkNotificationHr(history->RemoveGroupedTagWithId(
                NotificationString(notificationTag(id)), NotificationString(L"oneui"),
                NotificationString(unicode::fromUtf8(id_))));
            callback({NotificationStatus::Accepted, id, "Withdrawal accepted by Windows"});
        } catch (const std::exception &error) {
            callback({NotificationStatus::Failed, id, error.what()});
        }
    }
};
} // namespace
std::shared_ptr<NotificationBackend>
makeNotificationBackend(const ApplicationOptions &options,
                        NotificationBackend::Activation activation) {
    return std::make_shared<WindowsNotifications>(options, std::move(activation));
}
} // namespace oneui::platform

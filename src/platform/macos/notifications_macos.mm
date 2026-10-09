#include "internal/runtime_error.h"
#include "internal/unicode.h"
#include "platform/shared/notification_backend.h"
#import <Cocoa/Cocoa.h>
#import <UserNotifications/UserNotifications.h>
#include <deque>

namespace {
NSString *ns(const std::string &text) {
    return [[NSString alloc] initWithBytes:text.data()
                                    length:text.size()
                                  encoding:NSUTF8StringEncoding];
}
NSString *ns(const std::wstring &text) { return ns(oneui::unicode::toUtf8(text)); }
std::string utf8(NSString *value) {
    return value ? std::string(value.UTF8String ?: "") : std::string{};
}
oneui::NotificationPermission permission(UNAuthorizationStatus value) {
    switch (value) {
    case UNAuthorizationStatusNotDetermined:
        return oneui::NotificationPermission::NotDetermined;
    case UNAuthorizationStatusDenied:
        return oneui::NotificationPermission::Denied;
    case UNAuthorizationStatusAuthorized:
    case UNAuthorizationStatusProvisional:
        return oneui::NotificationPermission::Granted;
    default:
        return oneui::NotificationPermission::Unknown;
    }
}
} // namespace
@interface OneUiNotificationDelegate : NSObject <UNUserNotificationCenterDelegate> {
  @public
    oneui::platform::NotificationBackend::Activation activate;
    NSString *applicationId;
    __weak id<UNUserNotificationCenterDelegate> previous;
}
@end
@implementation OneUiNotificationDelegate
- (void)userNotificationCenter:(UNUserNotificationCenter *)center
       willPresentNotification:(UNNotification *)notification
         withCompletionHandler:(void (^)(UNNotificationPresentationOptions))completion {
    if ([notification.request.content.userInfo[@"oneui.application"] isEqual:applicationId]) {
        completion(UNNotificationPresentationOptionBanner | UNNotificationPresentationOptionList);
    } else if ([previous respondsToSelector:_cmd]) {
        [previous userNotificationCenter:center
                 willPresentNotification:notification
                   withCompletionHandler:completion];
    } else
        completion(UNNotificationPresentationOptionNone);
}
- (void)userNotificationCenter:(UNUserNotificationCenter *)center
    didReceiveNotificationResponse:(UNNotificationResponse *)response
             withCompletionHandler:(void (^)(void))completion {
    if (![response.notification.request.content.userInfo[@"oneui.application"]
            isEqual:applicationId]) {
        if ([previous respondsToSelector:_cmd])
            [previous userNotificationCenter:center
                didReceiveNotificationResponse:response
                         withCompletionHandler:completion];
        else
            completion();
        return;
    }
    if (![response.actionIdentifier isEqual:UNNotificationDismissActionIdentifier]) {
        oneui::ApplicationActivation value;
        if (oneui::platform::decodeActivation(
                utf8(response.notification.request.content.userInfo[@"oneui.activation"]), value)) {
            if (![response.actionIdentifier isEqual:UNNotificationDefaultActionIdentifier])
                value.actionId = utf8(response.actionIdentifier);
            try {
                if (activate)
                    activate(std::move(value));
            } catch (...) {
                oneui::detail::recordCurrentException("Notification.activate");
            }
        }
    }
    completion();
}
@end

namespace oneui::platform {
namespace {
class CocoaNotifications final : public NotificationBackend,
                                 public std::enable_shared_from_this<CocoaNotifications> {
    UNUserNotificationCenter *center_ = nil;
    OneUiNotificationDelegate *delegate_ = nil;
    id<UNUserNotificationCenterDelegate> previous_ = nil;
    NSString *appId_;
    struct Pending {
        Notification value;
        Result callback;
        bool withdrawal = false;
    };
    std::deque<Pending> queue_;
    void next() {
        if (queue_.empty())
            return;
        if (queue_.front().withdrawal) {
            auto id = ns(queue_.front().value.id);
            [center_ removePendingNotificationRequestsWithIdentifiers:@[ id ]];
            [center_ removeDeliveredNotificationsWithIdentifiers:@[ id ]];
            finish(NotificationStatus::Accepted, "Withdrawal submitted to UserNotifications");
            return;
        }
        auto weak = weak_from_this();
        [center_ getNotificationSettingsWithCompletionHandler:^(UNNotificationSettings *settings) {
          dispatch_async(dispatch_get_main_queue(), ^{
            auto self = weak.lock();
            if (!self || self->queue_.empty())
                return;
            if (::permission(settings.authorizationStatus) != NotificationPermission::Granted) {
                self->finish(NotificationStatus::PermissionDenied,
                             "Notification permission has not been granted");
                return;
            }
            self->submit();
          });
        }];
    }
    void finish(NotificationStatus status, std::string detail) {
        if (queue_.empty())
            return;
        auto item = std::move(queue_.front());
        queue_.pop_front();
        item.callback({status, item.value.id, std::move(detail)});
        next();
    }
    void submit() {
        const auto value = queue_.front().value;
        UNMutableNotificationContent *content = [UNMutableNotificationContent new];
        content.title = ns(value.title);
        content.body = ns(value.body);
        content.userInfo = @{
            @"oneui.application" : appId_,
            @"oneui.activation" : ns(encodeActivation({value.id, {}, value.parameter}))
        };
        // The complete category ID is stable, including the action labels. No
        // process-random hash is used, so delivered buttons survive a restart.
        std::string key = encodeActivation({value.id, {}, {}});
        NSMutableArray<UNNotificationAction *> *actions = [NSMutableArray array];
        for (const auto &action : value.actions) {
            key += ":" + encodeActivation({action.id, {}, unicode::toUtf8(action.title)});
            [actions addObject:[UNNotificationAction
                                   actionWithIdentifier:ns(action.id)
                                                  title:ns(action.title)
                                                options:UNNotificationActionOptionForeground]];
        }
        content.categoryIdentifier = ns(key);
        auto *category =
            [UNNotificationCategory categoryWithIdentifier:content.categoryIdentifier
                                                   actions:actions
                                         intentIdentifiers:@[]
                                                   options:UNNotificationCategoryOptionNone];
        auto weak = weak_from_this();
        [center_ getNotificationCategoriesWithCompletionHandler:^(
                     NSSet<UNNotificationCategory *> *existing) {
          dispatch_async(dispatch_get_main_queue(), ^{
            auto self = weak.lock();
            if (!self)
                return;
            NSMutableSet<UNNotificationCategory *> *categories = [existing mutableCopy];
            if (![categories containsObject:category] && categories.count >= 128) {
                self->finish(NotificationStatus::Failed,
                             "Notification category capacity reached (128)");
                return;
            }
            [categories addObject:category];
            [self->center_ setNotificationCategories:categories];
            auto *request = [UNNotificationRequest requestWithIdentifier:ns(value.id)
                                                                 content:content
                                                                 trigger:nil];
            [self->center_ addNotificationRequest:request
                            withCompletionHandler:^(NSError *error) {
                              const auto message = error ? utf8(error.localizedDescription)
                                                         : "Request accepted by UserNotifications";
                              const auto status =
                                  error ? NotificationStatus::Failed : NotificationStatus::Accepted;
                              dispatch_async(dispatch_get_main_queue(), ^{
                                if (auto alive = weak.lock())
                                    alive->finish(status, message);
                              });
                            }];
          });
        }];
    }

  public:
    CocoaNotifications(const ApplicationOptions &options, Activation activation)
        : appId_(ns(options.id)) {
        // UNUserNotificationCenter aborts for unbundled tools on some releases.
        // Check a matching .app identity before touching the singleton.
        if (![NSThread isMainThread] ||
            ![NSBundle.mainBundle.bundleURL.pathExtension isEqual:@"app"] ||
            ![NSBundle.mainBundle.bundleIdentifier isEqual:appId_])
            return;
        center_ = UNUserNotificationCenter.currentNotificationCenter;
        previous_ = center_.delegate;
        delegate_ = [OneUiNotificationDelegate new];
        delegate_->applicationId = appId_;
        delegate_->activate = std::move(activation);
        delegate_->previous = previous_;
        center_.delegate = delegate_;
    }
    ~CocoaNotifications() override {
        if (center_.delegate == delegate_)
            center_.delegate = previous_;
    }
    void permission(bool request, Permission callback) override {
        if (!center_) {
            callback({NotificationPermission::Unknown, NotificationStatus::ServiceUnavailable,
                      "Launch from a .app bundle whose identifier matches Application.id"});
            return;
        }
        auto center = center_;
        auto query = [center, callback] {
            [center
                getNotificationSettingsWithCompletionHandler:^(UNNotificationSettings *settings) {
                  const auto value = ::permission(settings.authorizationStatus);
                  callback({value,
                            value == NotificationPermission::Denied
                                ? NotificationStatus::PermissionDenied
                                : NotificationStatus::Accepted,
                            {}});
                }];
        };
        if (!request) {
            query();
            return;
        }
        [center
            requestAuthorizationWithOptions:UNAuthorizationOptionAlert | UNAuthorizationOptionSound
                          completionHandler:^(BOOL, NSError *error) {
                            if (error)
                                callback({NotificationPermission::Unknown,
                                          NotificationStatus::Failed,
                                          utf8(error.localizedDescription)});
                            else
                                query();
                          }];
    }
    void send(const Notification &value, Result callback) override {
        if (!center_) {
            callback({NotificationStatus::ServiceUnavailable, value.id,
                      "Matching application bundle required"});
            return;
        }
        if (queue_.size() >= 64) {
            callback(
                {NotificationStatus::Failed, value.id, "Notification submission queue is full"});
            return;
        }
        queue_.push_back({value, std::move(callback)});
        if (queue_.size() == 1)
            next();
    }
    void withdraw(const std::string &id, Result callback) override {
        if (!center_) {
            callback({NotificationStatus::ServiceUnavailable, id,
                      "Matching application bundle required"});
            return;
        }
        if (queue_.size() >= 64) {
            callback({NotificationStatus::Failed, id, "Notification submission queue is full"});
            return;
        }
        Notification value;
        value.id = id;
        queue_.push_back({std::move(value), std::move(callback), true});
        if (queue_.size() == 1)
            next();
    }
};
} // namespace
std::shared_ptr<NotificationBackend>
makeNotificationBackend(const ApplicationOptions &options,
                        NotificationBackend::Activation activation) {
    return std::make_shared<CocoaNotifications>(options, std::move(activation));
}
} // namespace oneui::platform

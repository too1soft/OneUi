#include <oneui/notification.h>

bool example_notification(oneui::NotificationService &service) {
    // Keep service and its Application alive while waiting for callbacks.
    oneui::Notification message;
    message.id = "task-42";
    message.title = L"任务完成";
    message.body = L"发送同一 ID 会替换此前的通知。";
    message.parameter = "features/notifications";
    message.actions = {{"open", L"查看"}};
    return service.send(std::move(message), [](const oneui::NotificationResult &result) {
        // Inspect result.status and result.detail. Accepted means the request
        // was accepted, not that the user saw it. Clicks reach Application.
        (void)result;
    });
}

#include <oneui/controls/popup.h>
// Retain the returned popup/content for reopening. Coordinates are in the host viewport.
std::shared_ptr<oneui::Popup> example_popup(const std::shared_ptr<oneui::OverlayHost> &host,
                                            const std::shared_ptr<oneui::Widget> &content,
                                            oneui::Rect anchor) {
    auto popup = std::make_shared<oneui::Popup>();
    popup->setContent(content);
    popup->setAnchorRect(anchor);
    popup->setInteractionMode(oneui::PopupInteractionMode::LightDismiss);
    popup->setOpen(true);
    // Fill supplies a viewport; PopupPlacement positions and flips the content.
    host->addAnchoredOverlay(popup, popup->overlayOptions(10), {-1, -1}, {}, 0, 0);
    return popup;
}
void example_close_popup(oneui::OverlayHost &host, oneui::Popup &popup) {
    popup.setOpen(false);
    host.removeOverlay(&popup); // Deactivate transient UI when its page leaves.
}

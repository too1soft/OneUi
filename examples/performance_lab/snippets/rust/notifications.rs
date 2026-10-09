use oneui::{Notification, NotificationAction, NotificationService};

pub fn notification(service: &NotificationService) -> bool {
    service.send(
        &Notification {
            id: "task-42".into(),
            title: "任务完成".into(),
            body: "发送同一 ID 会替换此前的通知。".into(),
            parameter: "features/notifications".into(),
            actions: vec![NotificationAction {
                id: "open".into(),
                title: "查看".into(),
            }],
        },
        |result| {
            // Accepted means accepted, not displayed. Clicks reach Application.
            println!("notification result: {:?}", result);
        },
    )
}

use oneui::{Error, Insets, OverlayAlignment, OverlayHost, Popup, PopupInteractionMode, Widget};

// Keep this wrapper and any content callback owners alive while the popup is mounted.
pub fn popup(host: &OverlayHost, content: &Widget, anchor: (f32, f32, f32, f32)) -> Result<Popup, Error> {
    let popup = Popup::new()?;
    popup.set_content(content);
    popup.set_anchor_rect(anchor.0, anchor.1, anchor.2, anchor.3);
    popup.set_interaction_mode(PopupInteractionMode::LightDismiss);
    popup.set_open(true);
    host.add_anchored_overlay(popup.as_widget(), 10, -1.0, -1.0, Insets::default(),
                              OverlayAlignment::Start, OverlayAlignment::Start);
    Ok(popup)
}
pub fn close_popup(host: &OverlayHost, popup: &Popup) {
    popup.set_open(false);
    host.remove_overlay(popup.as_widget());
}

#include <oneui/oneui_c_api.h>
#include <stdio.h>
#define UTF8(s) ((OneUiUtf8String){s, sizeof(s) - 1})

static void notification_result(int status, OneUiUtf8String id, OneUiUtf8String detail,
                                void *data) {
    (void)data;
    /* Borrowed strings are valid only during the callback. 0 = request accepted,
       not proof of display. Clicks reach the Application activation handler. */
    printf("status=%d id=%.*s detail=%.*s\n", status, (int)id.length, id.data, (int)detail.length,
           detail.data);
}
int example_c_notification(OneUiNotificationService *service) {
    OneUiNotificationActionUtf8 action = {UTF8("open"), UTF8("查看")};
    OneUiNotificationUtf8 message = {UTF8("task-42"),
                                     UTF8("任务完成"),
                                     UTF8("发送同一 ID 会替换此前的通知。"),
                                     UTF8("features/notifications"),
                                     &action,
                                     1};
    /* Input data is copied. Retain service/app for asynchronous completion. */
    return oneui_notification_send_utf8(service, &message, notification_result, NULL, NULL);
}

/* The host retains native children. Retain/release the returned handle normally.
   anchor is in host coordinates; content needs a preferred size. */
OneUiWidget *example_c_popup(OneUiWidget *host, OneUiWidget *content, float x, float y, float width,
                             float height) {
    OneUiWidget *popup = oneui_popup_create();
    if (!popup)
        return NULL;
    oneui_popup_set_content(popup, content);
    oneui_popup_set_anchor_rect(popup, x, y, width, height);
    oneui_popup_set_interaction_mode(popup, 1); /* LightDismiss */
    oneui_popup_set_open(popup, 1);
    OneUiInsets margin = {0, 0, 0, 0};
    oneui_overlay_host_add_anchored_overlay(host, popup, 10, -1, -1, margin, 0, 0);
    return popup;
}
void example_c_close_popup(OneUiWidget *host, OneUiWidget *popup) {
    oneui_popup_set_open(popup, 0);
    oneui_overlay_host_remove_overlay(host, popup);
    oneui_widget_destroy(popup); /* Only after the caller stops reusing the handle. */
}


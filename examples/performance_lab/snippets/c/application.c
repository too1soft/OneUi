#include <oneui/oneui_c_api.h>
#define UTF8(s) ((OneUiUtf8String){s, sizeof(s) - 1})

static void accept_close(OneUiCloseDecision *decision, void *data) {
    (void)data;
    /* Retain the decision for an async UI if necessary. Resolve consumes it;
       oneui_close_decision_destroy instead cancels and consumes it. */
    oneui_close_decision_resolve(decision, 1);
}
int example_c_application(void) {
    int code = -1;
    OneUiApplication *app = oneui_application_create_utf8(UTF8("org.example.oneui"), UTF8("示例"));
    if (!app)
        return code;
    if (!oneui_application_is_primary(app)) {
        oneui_application_destroy(app);
        return 0;
    }
    OneUiWindowOptionsUtf8 options = {0};
    options.title = UTF8("主窗口");
    options.width = 800;
    options.height = 600;
    options.resizable = 1;
    OneUiWindow *window = oneui_window_create_utf8(&options);
    if (window && oneui_window_initialize_checked(window) &&
        oneui_application_add_window(app, window, 1)) {
        oneui_application_set_close_guard(app, window, accept_close, NULL, NULL);
        oneui_window_show(window);
        code = oneui_application_run(app);
    }
    /* Registered windows remain caller-owned; destroy them before the app. */
    if (window)
        oneui_window_destroy(window);
    oneui_application_destroy(app);
    return code;
}

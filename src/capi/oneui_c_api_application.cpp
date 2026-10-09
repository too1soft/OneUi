#include "oneui/application.h"
#include "oneui_c_api_application_internal.h"
#include "oneui_c_api_desktop_internal.h"

using namespace oneui::capi;
struct OneUiCloseDecision {
    std::function<void(bool)> answer;
};
namespace {
oneui::Application &application(OneUiApplication *value) { return desktopApplication(value); }
} // namespace
OneUiApplication *oneui_application_create_utf8(OneUiUtf8String id, OneUiUtf8String name) {
    try {
        auto result = std::make_unique<OneUiApplication>();
        result->impl = std::make_unique<oneui::Application>(oneui::ApplicationOptions{
            desktopString(id), oneui::unicode::fromUtf8(desktopString(name))});
        return result.release();
    } catch (...) {
        recordCurrentException(__func__);
        return nullptr;
    }
}
void oneui_application_destroy(OneUiApplication *app) { delete app; }
int oneui_application_add_window(OneUiApplication *app, OneUiWindow *window, int main) {
    return desktopCall(
        __func__, [&] { return application(app).addWindow(desktopWindow(window), main != 0); });
}
int oneui_application_remove_window(OneUiApplication *app, OneUiWindow *window) {
    return desktopCall(__func__,
                       [&] { return application(app).removeWindow(desktopWindow(window)); });
}
int oneui_application_set_quit_policy(OneUiApplication *app, int policy) {
    return desktopCall(__func__, [&] {
        if (policy < 0 || policy > 2)
            throw std::invalid_argument("Invalid quit policy");
        application(app).setQuitPolicy(oneui::QuitPolicy(policy));
        return true;
    });
}
int oneui_application_run(OneUiApplication *app) {
    try {
        return application(app).run();
    } catch (...) {
        recordCurrentException(__func__);
        return -1;
    }
}
int oneui_application_request_quit(OneUiApplication *app) {
    return desktopCall(__func__, [&] {
        application(app).requestQuit();
        return true;
    });
}
int oneui_application_set_close_guard(OneUiApplication *app, OneUiWindow *window,
                                      OneUiCloseGuardCallback callback, void *data,
                                      OneUiDestroyCallback cleanup) {
    return desktopCall(__func__, [&] {
        auto owned = ownDesktopCallback(data, cleanup);
        if (!callback) {
            application(app).setCloseGuard(desktopWindow(window), {});
            return true;
        }
        application(app).setCloseGuard(desktopWindow(window), [owned, callback](auto answer) {
            auto decision = std::make_unique<OneUiCloseDecision>();
            decision->answer = std::move(answer);
            callback(decision.release(), owned->data);
        });
        return true;
    });
}
int oneui_application_set_on_activation(OneUiApplication *app,
                                        OneUiApplicationActivationCallback callback, void *data,
                                        OneUiDestroyCallback cleanup) {
    return desktopCall(__func__, [&] {
        auto owned = ownDesktopCallback(data, cleanup);
        if (!callback) {
            application(app).setOnActivation({});
            return true;
        }
        application(app).setOnActivation(
            [owned, callback](const oneui::ApplicationActivation &value) {
                callback({value.notificationId.data(), value.notificationId.size()},
                         {value.actionId.data(), value.actionId.size()},
                         {value.parameter.data(), value.parameter.size()}, owned->data);
            });
        return true;
    });
}
void oneui_close_decision_resolve(OneUiCloseDecision *value, int accept) {
    std::unique_ptr<OneUiCloseDecision> decision(value);
    desktopCall(__func__, [&] {
        if (decision && decision->answer)
            decision->answer(accept != 0);
        return true;
    });
}
void oneui_close_decision_destroy(OneUiCloseDecision *value) {
    oneui_close_decision_resolve(value, 0);
}

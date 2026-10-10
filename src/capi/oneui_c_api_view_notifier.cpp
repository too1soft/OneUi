#include "oneui_c_api_view_internal.h"
namespace oneui::capi::views {
std::pair<std::shared_ptr<Runtime>, std::shared_ptr<Scope>> notifier(OneUiViewNotifier *n) {
    check(n, ONEUI_VIEW_INVALID_ARGUMENT, "Notifier required");
    check(n->thread == std::this_thread::get_id(), ONEUI_VIEW_WRONG_THREAD,
          "Notifier requires UI thread");
    auto view = n->runtime.lock();
    auto scope = n->scope.lock();
    check(view && scope && !scope->closed, ONEUI_VIEW_CLOSED, "Notifier disconnected");
    view->alive();
    return {view, scope};
}
} // namespace oneui::capi::views
using namespace oneui::capi::views;
OneUiViewResult oneui_view_notifier(OneUiView *view, OneUiViewNotifier **out) {
    if (out)
        *out = nullptr;
    return call(__func__, [&] {
        check(out, ONEUI_VIEW_INVALID_ARGUMENT, "Notifier output required");
        auto value = runtime(view);
        value->alive();
        *out = value->rootScope->notifier;
        oneui_view_notifier_retain(*out);
    });
}
void oneui_view_notifier_retain(OneUiViewNotifier *n) {
    if (n)
        ++n->references;
}
void oneui_view_notifier_release(OneUiViewNotifier *n) {
    if (n && --n->references == 0)
        delete n;
}
OneUiViewResult oneui_view_notifier_notify(OneUiViewNotifier *n, uint32_t property) {
    return call(__func__, [&] {
        auto pair = notifier(n);
        pair.first->notify(pair.second, property);
    });
}
OneUiViewResult oneui_view_notifier_notify_list(OneUiViewNotifier *n, uint32_t property,
                                                const OneUiViewListChange *change) {
    return call(__func__, [&] {
        check(change, ONEUI_VIEW_INVALID_ARGUMENT, "Change required");
        auto pair = notifier(n);
        pair.first->notifyList(pair.second, property, *change);
    });
}

#include "oneui_c_api_view_internal.h"
#include <mutex>
namespace oneui::capi::views {
namespace {
std::mutex hostMutex;
std::map<OneUiWidget *, std::weak_ptr<Runtime>> hostOwners;
struct HostLease {
    void *owner;
    OneUiDestroyCallback release;
    std::weak_ptr<Runtime> runtime;
    bool retained = false;
    ~HostLease() {
        if (!retained)
            return;
        try {
            if (auto value = runtime.lock()) {
                Callback guard(*value, Phase::Cleanup);
                release(owner);
            } else
                release(owner);
        } catch (...) {
            recordError(OneUiErrorOperationFailed, "host release", "Foreign owner release threw");
        }
    }
};
} // namespace
void bindHostHandle(OneUiWidget *host, const std::shared_ptr<Runtime> &runtime) {
    std::lock_guard<std::mutex> lock(hostMutex);
    hostOwners[host] = runtime;
}
void unbindHostHandle(OneUiWidget *host) {
    std::lock_guard<std::mutex> lock(hostMutex);
    hostOwners.erase(host);
}
std::shared_ptr<Runtime> hostRuntime(OneUiWidget *host) {
    std::shared_ptr<Runtime> value;
    {
        std::lock_guard<std::mutex> lock(hostMutex);
        auto found = hostOwners.find(host);
        if (found != hostOwners.end())
            value = found->second.lock();
    }
    check(bool(value), ONEUI_VIEW_CLOSED, "Host is not a live view host");
    value->onThread();
    value->alive();
    check(callbackPhase == Phase::Idle, ONEUI_VIEW_REENTRANT, "Host change during callback");
    return value;
}
} // namespace oneui::capi::views
using namespace oneui::capi::views;
OneUiViewResult oneui_view_find_host(OneUiView *view, OneUiViewNotifier *scope,
                                     OneUiUtf8String name, OneUiWidget **out) {
    if (out)
        *out = nullptr;
    return call(__func__, [&] {
        check(out && !scope, ONEUI_VIEW_INVALID_ARGUMENT, "WP3 supports only root host scope");
        auto value = runtime(view);
        value->alive();
        auto key = text(name);
        auto found = value->hosts.find(key);
        check(found != value->hosts.end(), ONEUI_VIEW_NOT_FOUND, "Host not found");
        auto &handle = view->hosts[key];
        if (!handle) {
            handle = std::make_unique<OneUiWidget>();
            handle->widget = found->second;
            bindHostHandle(handle.get(), value);
        }
        *out = handle.get();
    });
}
OneUiViewResult oneui_view_host_attach(OneUiWidget *host, OneUiWidget *content, void *owner,
                                       OneUiVoidCallback retain, OneUiDestroyCallback release) {
    return call(__func__, [&] {
        auto value = hostRuntime(host);
        check(content && content->widget && owner && retain && release, ONEUI_VIEW_INVALID_ARGUMENT,
              "Content and owner callbacks required");
        auto native = std::dynamic_pointer_cast<oneui::ui::NativeHostView>(host->widget);
        check(native && native->children().empty() && !content->widget->hasCompositionOwner(),
              ONEUI_VIEW_BUSY, "Host/content already mounted");
        auto lease = std::make_shared<HostLease>();
        lease->owner = owner;
        lease->release = release;
        lease->runtime = value;
        {
            Callback guard(*value, Phase::Cleanup);
            retain(owner);
            lease->retained = true;
        }
        value->alive();
        native->attach(content->widget, lease);
        value->styles->add(oneui::ui::Element(oneui::ui::Node(native), "NativeHost"));
    });
}
OneUiViewResult oneui_view_host_detach(OneUiWidget *host) {
    return call(__func__, [&] {
        auto value = hostRuntime(host);
        auto native = std::dynamic_pointer_cast<oneui::ui::NativeHostView>(host->widget);
        check(bool(native), ONEUI_VIEW_INVALID_ARGUMENT, "Native host required");
        native->detach();
    });
}

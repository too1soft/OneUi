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
void pruneHostHandles(OneUiView *view) {
    for (const auto &[_, handle] : view->hosts)
        if (handle->widget && !std::any_of(view->value->hosts.begin(), view->value->hosts.end(),
            [&](const auto &entry) { return entry.second == handle->widget; })) {
            unbindHostHandle(handle.get());
            handle->widget.reset();
        }
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
    check(std::any_of(value->hosts.begin(), value->hosts.end(),
        [host](const auto &entry) { return entry.second == host->widget; }),
        ONEUI_VIEW_CLOSED, "Host instance was removed");
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
        check(out, ONEUI_VIEW_INVALID_ARGUMENT, "Host output required");
        auto value = runtime(view);
        value->alive();
        check(callbackPhase == Phase::Idle, ONEUI_VIEW_REENTRANT, "Host lookup during callback");
        auto key = text(name);
        if (scope) {
            auto [owner, row] = notifier(scope);
            check(owner == value, ONEUI_VIEW_INVALID_ARGUMENT, "Scope belongs to another View");
            check(row == value->rootScope || row->renderScope, ONEUI_VIEW_INVALID_ARGUMENT,
                  "Host lookup requires root or workspace render scope");
            key = Runtime::hostKey(row->path, key);
        }
        auto found = value->hosts.find(key);
        check(found != value->hosts.end(), ONEUI_VIEW_NOT_FOUND, "Host not found");
        pruneHostHandles(view);
        check(view->hosts.count(found->second.get()) || view->retiredHosts.size() + view->hosts.size() < 100000,
            ONEUI_VIEW_LIMIT, "Host handle budget; recreate the View");
        auto &handle = view->hosts[found->second.get()];
        if (handle && !handle->widget) {
            check(view->retiredHosts.size() + view->hosts.size() < 100000,
                ONEUI_VIEW_LIMIT, "Retired host handle limit; recreate the View");
            view->retiredHosts.push_back(std::move(handle));
        }
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

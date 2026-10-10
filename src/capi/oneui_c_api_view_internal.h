#pragma once
#include "../views/runtime.h"
#include "oneui/layout/panel.h"
#include "oneui_c_api_error.h"
#include "oneui_c_api_widget_internal.h"
#include "oneui_c_api_window_internal.h"
namespace oneui::capi::views {
using namespace oneui::views;
struct Mount : std::enable_shared_from_this<Mount> {
    std::thread::id thread = std::this_thread::get_id();
    OneUiWindow *window = nullptr;
    std::shared_ptr<Panel> destination;
    std::shared_ptr<Widget> windowRoot;
    Window::EventSubscription closedSubscription;
    std::weak_ptr<Runtime> active;
    std::string externalCss;
    ui::Density density = ui::Density::Comfortable;
    bool closed = false, reserved = false;
    void check() const;
    void close(bool windowClosed = false);
    void publish(const std::shared_ptr<Runtime> &);
};
template <class F> int call(const char *operation, F action) noexcept {
    try {
        action();
        return ONEUI_VIEW_OK;
    } catch (const Failure &e) {
        recordError(OneUiErrorOperationFailed, operation, e.what());
        return e.result;
    } catch (const std::bad_alloc &) {
        recordError(OneUiErrorOutOfMemory, operation, "View allocation failed");
        return ONEUI_VIEW_OUT_OF_MEMORY;
    } catch (const oneview::Error &e) {
        recordError(OneUiErrorInvalidArgument, operation, e.what());
        return std::strstr(e.what(), "VERSION:") ? ONEUI_VIEW_VERSION : ONEUI_VIEW_BAD_IR;
    } catch (const std::exception &e) {
        recordError(OneUiErrorOperationFailed, operation, e.what());
        return ONEUI_VIEW_FOREIGN_FAILURE;
    } catch (...) {
        recordError(OneUiErrorOperationFailed, operation, "View operation failed");
        return ONEUI_VIEW_FOREIGN_FAILURE;
    }
}
std::shared_ptr<Runtime> runtime(OneUiView *);
std::pair<std::shared_ptr<Runtime>, std::shared_ptr<Scope>> notifier(OneUiViewNotifier *);
std::string text(OneUiUtf8String);
bool contains(const std::shared_ptr<Widget> &root, const Widget *target);
void bindHostHandle(OneUiWidget *, const std::shared_ptr<Runtime> &);
void unbindHostHandle(OneUiWidget *);
void pruneHostHandles(OneUiView *);
std::shared_ptr<Runtime> hostRuntime(OneUiWidget *);
void registerMount(OneUiWindow *, const std::shared_ptr<Mount> &);
bool closeWindowMounts(OneUiWindow *) noexcept;
} // namespace oneui::capi::views
struct OneUiViewMount {
    std::shared_ptr<oneui::capi::views::Mount> value;
};
struct OneUiView {
    std::shared_ptr<oneui::views::Runtime> value;
    OneUiWidget root;
    std::map<const oneui::Widget *, std::unique_ptr<OneUiWidget>> hosts;
    std::vector<std::unique_ptr<OneUiWidget>> retiredHosts;
    ~OneUiView() {
        for (auto &entry : hosts)
            oneui::capi::views::unbindHostHandle(entry.second.get());
    }
};

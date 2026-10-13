#include "oneui/ui_theme.h"
#include "oneui/layout/overlay_host.h"
#include "oneui/controls/popup.h"
#include "oneui_c_api_view_internal.h"
namespace oneui::capi::views {
std::string text(OneUiUtf8String s) {
    OneUiValue value{};
    value.tag = 4;
    value.as.utf8 = {s.data, s.length};
    return Value::copy(value).text;
}
bool contains(const std::shared_ptr<Widget> &root, const Widget *target) {
    if (!root)
        return false;
    if (root.get() == target)
        return true;
    if (auto host = std::dynamic_pointer_cast<OverlayHost>(root)) {
        if (contains(host->content(), target)) return true;
        for (auto &entry : host->overlays())
            if (contains(entry.child, target)) return true;
    }
    if (auto popup = std::dynamic_pointer_cast<Popup>(root))
        if (contains(popup->anchor(), target) || contains(popup->content(), target))
            return true;
    if (auto view = std::dynamic_pointer_cast<View>(root))
        for (auto &child : view->children())
            if (contains(child, target))
                return true;
    return false;
}
void Mount::check() const {
    oneui::views::check(thread == std::this_thread::get_id(), ONEUI_VIEW_WRONG_THREAD,
                        "Mount requires UI thread");
    oneui::views::check(!closed && window && window->window, ONEUI_VIEW_CLOSED, "Mount closed");
    if (child) {
        auto parent = parentView.lock();
        oneui::views::check(parent && !parent->closing && parent->root &&
            contains(parent->root->element.widget, destination.get()),
            ONEUI_VIEW_CLOSED, "Parent view or child destination removed");
    }
}
void Mount::closeChildren() {
    auto values = std::move(children);
    for (auto& weak : values) if (auto value = weak.lock()) value->close();
}
void Mount::close(bool windowClosed) {
    if (closed)
        return;
    closed = true;
    closeChildren();
    if (windowClosed && windowRoot) {
        windowRoot->setInvalidator({});
        windowRoot->setRectInvalidator({});
        windowRoot->setAnimationScheduler({});
    }
    if (windowClosed)
        window = nullptr; // Observer may run during Window's base destructor.
    if (auto view = active.lock())
        view->close();
    window = nullptr;
    destination.reset();
    windowRoot.reset();
    closedSubscription.reset();
}
void Mount::publish(const std::shared_ptr<Runtime> &view) {
    check();
    view->styles->inheritTransient(window->styleSheet ? window->styleSheet->transientStyleSheet() : nullptr);
    active = view;
    if (!destination)
        windowRoot = view->root->element.widget;
    auto weak = weak_from_this();
    view->republish=[weak,runtime=std::weak_ptr<Runtime>(view)] {
        auto mount=weak.lock();auto value=runtime.lock();
        oneui::views::check(bool(mount) && bool(value),ONEUI_VIEW_CLOSED,"Reload mount closed");
        mount->check();
        // A reload is a new composition boundary; clients can reattach children
        // after successful reload. Ordinary property flushes do not call this.
        mount->closeChildren();
        auto root=value->root->element.widget;
        if(mount->destination) {
            if(mount->destination->children().empty() || mount->destination->children().front()!=root)
                mount->destination->reconcileChildren({root});
        } else if(mount->windowRoot!=root) {
            mount->window->window->setContent(root);mount->window->rootContent=root;mount->windowRoot=root;
        }
    };
    view->detach = [weak] {
        if (auto mount = weak.lock()) {
            mount->closeChildren();
            if (mount->destination)
                mount->destination->reconcileChildren({});
            else if (mount->window && mount->window->window) {
                mount->window->window->setContent(nullptr);
                mount->window->rootContent.reset();
            }
            mount->active.reset();
        }
    };
    view->schedule = [weak, runtime = std::weak_ptr<Runtime>(view)] {
        auto mount = weak.lock();
        if (!mount || mount->closed || !mount->window)
            return;
        if (!mount->window->window->post([weak, runtime] {
                auto mount = weak.lock();
                auto view = runtime.lock();
                if (!mount || mount->closed || !view || view->closing)
                    return;
                if (callbackRuntime) {
                    view->scheduled = false;
                    callbackRuntime->deferredSchedules.push_back(view);
                    return;
                }
                const auto sequence = view->error.sequence;
                try {
                    view->flush();
                } catch (const Failure &e) {
                    if (view->error.sequence == sequence)
                        view->fail(e.result, e.what());
                } catch (const std::bad_alloc &) {
                    view->fail(ONEUI_VIEW_OUT_OF_MEMORY, "Asynchronous view allocation failed");
                } catch (...) {
                    view->fail(ONEUI_VIEW_FOREIGN_FAILURE, "Asynchronous view flush failed");
                }
            }))
            if (auto view = runtime.lock())
                view->close();
    };
    if (destination)
        destination->reconcileChildren({view->root->element.widget});
    else {
        window->window->setContent(view->root->element.widget);
        window->rootContent = view->root->element.widget;
    }
    view->scheduled = false;
    view->enqueue();
}
} // namespace oneui::capi::views
using namespace oneui::capi::views;
OneUiViewResult oneui_view_mount_create(OneUiWindow *window, OneUiWidget *destination,
                                        OneUiViewMount **out) {
    if (out)
        *out = nullptr;
    return call(__func__, [&] {
        check(out && window && window->window, ONEUI_VIEW_INVALID_ARGUMENT, "Window/out required");
        bool closed;
        try {
            closed = window->window->closed();
        } catch (const std::logic_error &) {
            throw Failure(ONEUI_VIEW_WRONG_THREAD, "Window UI thread required");
        }
        check(!closed, ONEUI_VIEW_CLOSED, "Window closed");
        auto mount = std::make_shared<Mount>();
        mount->window = window;
        mount->windowRoot = window->rootContent;
        mount->externalCss = oneui::ui::declarativeTheme();
        if (destination) {
            mount->destination = std::dynamic_pointer_cast<oneui::Panel>(destination->widget);
            check(mount->destination && mount->destination->children().empty() &&
                      contains(window->rootContent, destination->widget.get()),
                  ONEUI_VIEW_INVALID_ARGUMENT, "Destination must be an empty panel in this window");
        } else
            check(!window->rootContent, ONEUI_VIEW_BUSY, "Window content slot occupied");
        auto weak = std::weak_ptr<Mount>(mount);
        mount->closedSubscription = window->window->observeClosed([weak] {
            if (auto mount = weak.lock())
                mount->close(true);
        });
        auto handle = std::make_unique<OneUiViewMount>();
        handle->value = std::move(mount);
        registerMount(window, handle->value);
        *out = handle.release();
    });
}
void oneui_view_mount_destroy(OneUiViewMount *handle) {
    if (!handle)
        return;
    call(__func__, [&] {
        check(handle->value->thread == std::this_thread::get_id(), ONEUI_VIEW_WRONG_THREAD,
              "Mount destroy requires UI thread");
        auto mount = handle->value;
        delete handle;
        mount->close();
    });
}
OneUiViewResult oneui_view_mount_set_styles(OneUiViewMount *handle, OneUiUtf8String css,
                                            uint32_t density) {
    return call(__func__, [&] {
        check(handle, ONEUI_VIEW_INVALID_ARGUMENT, "Mount required");
        auto mount = handle->value;
        mount->check();
        check(callbackPhase == Phase::Idle, ONEUI_VIEW_REENTRANT,
              "Style replacement during callback");
        check(density <= 1, ONEUI_VIEW_INVALID_ARGUMENT, "Density out of range");
        auto external = oneui::ui::syntax::css(text(css));
        auto combined = external;
        auto view = mount->active.lock();
        if (view)
            for (const auto &r : view->image.records[oneview::CSS])
                combined += '\n' + view->image.strings[r[2]];
        auto nativeDensity =
            density ? oneui::ui::Density::Compact : oneui::ui::Density::Comfortable;
        oneui::ui::StyleRegistry validation;
        validation.replace(combined, nativeDensity);
        if (view) {
            view->styles->replace(combined, nativeDensity);
            view->styles->inheritTransient(mount->window->styleSheet ? mount->window->styleSheet->transientStyleSheet() : nullptr);
            view->refreshRepeatLayouts(view->root);
        }
        mount->externalCss = std::move(external);
        mount->density = nativeDensity;
        if(view) {view->externalCss=mount->externalCss;view->density=nativeDensity;}
    });
}

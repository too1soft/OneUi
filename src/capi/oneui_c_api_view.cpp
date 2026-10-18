#include "oneui_c_api_view_internal.h"
// Included implementations provide stable-key workspace geometry and child mounts.
namespace oneui::capi::views {
std::shared_ptr<Runtime> runtime(OneUiView *view) {
    check(view && view->value, ONEUI_VIEW_INVALID_ARGUMENT, "View required");
    auto value = view->value;
    value->onThread();
    return value;
}
} // namespace oneui::capi::views
using namespace oneui::capi::views;
// Metrics also owns the additive focus and workspace-action capabilities.
#include "oneui_c_api_view_metrics.inc"
#include "oneui_c_api_view_child.inc"
#include "oneui_c_api_view_workspace.inc" // Window-checked native/declarative strip hosts.
#include "oneui_c_api_view_code_editor.inc"
uint32_t oneui_view_protocol_version(void) { return ONEUI_VIEW_PROTOCOL_VERSION; }
OneUiViewResult oneui_view_instantiate(OneUiViewMount *mountHandle, const uint8_t *bytes,
                                       size_t length, const OneUiViewModelVTable *table, void *vm,
                                       OneUiView **out) {
    if (out)
        *out = nullptr;
    return call(__func__, [&] {
        check(out && mountHandle && bytes && length && length <= oneview::max_file,
              ONEUI_VIEW_INVALID_ARGUMENT, "IR/mount/out required");
        auto mount = mountHandle->value;
        mount->check();
        check(callbackPhase == Phase::Idle, ONEUI_VIEW_REENTRANT, "Instantiate during callback");
        check(!mount->reserved && mount->active.expired(), ONEUI_VIEW_BUSY, "Mount already active");
        if (mount->destination)
            check(mount->destination->children().empty(), ONEUI_VIEW_BUSY, "Destination occupied");
        else
            check(!mount->window->rootContent, ONEUI_VIEW_BUSY, "Window content occupied");
        auto image = oneview::readImage(oneview::Bytes(bytes, bytes + length));
        auto view = std::make_shared<Runtime>(std::move(image), std::make_shared<oneui::SystemClipboard>());
        view->externalCss=mount->externalCss;
        view->density=mount->density;
        mount->reserved = true;
        mount->active = view;
        try {
            view->initialize(table, vm);
            mount->check();
            auto handle = std::make_unique<OneUiView>();
            handle->value = view;
            handle->root.widget = view->root->element.widget;
            mount->publish(view);
            view->alive();
            *out = handle.release();
            mount->reserved = false;
        } catch (...) {
            mount->reserved = false;
            view->close();
            throw;
        }
    });
}
OneUiWidget *oneui_view_root(OneUiView *view) {
    OneUiWidget *result = nullptr;
    call(__func__, [&] {
        auto value = runtime(view);
        value->alive();
        result = &view->root;
    });
    return result;
}
OneUiViewResult oneui_view_notify(OneUiView *view, uint32_t property) {
    return call(__func__, [&] {
        auto value = runtime(view);
        value->notify(value->rootScope, property);
    });
}
OneUiViewResult oneui_view_notify_list(OneUiView *view, uint32_t property,
                                       const OneUiViewListChange *change) {
    return call(__func__, [&] {
        check(change, ONEUI_VIEW_INVALID_ARGUMENT, "Change required");
        auto value = runtime(view);
        value->notifyList(value->rootScope, property, *change);
    });
}
OneUiViewResult oneui_view_flush(OneUiView *view) {
    return call(__func__, [&] {
        auto value = runtime(view);
        const auto sequence = value->error.sequence;
        try {
            value->flush();
            pruneHostHandles(view);
        } catch (const Failure &e) {
            if (value->error.sequence == sequence)
                value->fail(e.result, e.what());
            throw;
        } catch (const std::bad_alloc &) {
            value->fail(ONEUI_VIEW_OUT_OF_MEMORY, "View flush allocation failed");
            throw;
        }
    });
}
OneUiViewResult oneui_view_disconnect(OneUiView *view) {
    return call(__func__, [&] { runtime(view)->close(); });
}
void oneui_view_destroy(OneUiView *view) {
    if (!view)
        return;
    call(__func__, [&] {
        auto value = runtime(view);
        delete view;
        value->close();
    });
}
OneUiViewResult oneui_view_get_error(OneUiView *view, OneUiViewError *out) {
    return call(__func__, [&] {
        check(out && out->struct_size == sizeof(*out), ONEUI_VIEW_INVALID_ARGUMENT,
              "Error output size");
        auto value = runtime(view);
        *out = value->error;
    });
}

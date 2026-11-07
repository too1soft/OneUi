#include "runtime_internal.h"

namespace oneui::plugin {
namespace {
OneUiPluginResult detach(OneUiPluginRuntime &r) {
    for (auto i = r.order.rbegin(); i != r.order.rend(); ++i) {
        auto &e = **i;
        if (!e.attempted) continue;
        OneUiPluginResult result;
        try { result = e.detach(e.instance); }
        catch (...) { result = ONEUI_PLUGIN_FAILED; }
        if (result != ONEUI_PLUGIN_OK)
            return r.fail(result, "Plugin cleanup incomplete; stop can be retried", e.id.c_str());
        for (auto service = r.services.begin(); service != r.services.end();) {
            if (service->second.owner == &e) service = r.services.erase(service);
            else ++service;
        }
        e.instance = nullptr;
        e.attempted = false;
    }
    r.state = ONEUI_PLUGIN_STOPPED;
    return ONEUI_PLUGIN_OK;
}
}
OneUiPluginResult start(OneUiPluginRuntime &r) {
    if (r.state != ONEUI_PLUGIN_REGISTERING)
        return r.fail(ONEUI_PLUGIN_INVALID_STATE, "Plugin runtime has already started");
    const auto valid = resolve(r);
    if (valid != ONEUI_PLUGIN_OK) return valid;
    Operation operation(r);
    r.state = ONEUI_PLUGIN_STARTING;
    for (auto *entry : r.order) {
        auto &e = *entry;
        r.attaching = &e;
        e.attempted = true;
        OneUiPluginResult result;
        try { result = e.attach(&e.context, e.userData, &e.instance); }
        catch (...) { result = ONEUI_PLUGIN_FAILED; }
        r.attaching = nullptr;
        if (result != ONEUI_PLUGIN_OK) {
            const auto original = r.fail(result, "Plugin attach failed", e.id.c_str());
            const auto message = r.error; // Preserve the initiating failure if rollback succeeds.
            r.state = ONEUI_PLUGIN_STOPPING;
            const auto cleanup = detach(r);
            if (cleanup != ONEUI_PLUGIN_OK) return cleanup;
            r.error = message;
            return original;
        }
    }
    r.state = ONEUI_PLUGIN_ACTIVE;
    return ONEUI_PLUGIN_OK;
}
OneUiPluginResult stop(OneUiPluginRuntime &r) {
    if (r.state == ONEUI_PLUGIN_STOPPED) return ONEUI_PLUGIN_OK;
    if (r.leases) return r.fail(ONEUI_PLUGIN_BUSY, "Service leases must be released before stopping");
    Operation operation(r);
    r.state = ONEUI_PLUGIN_STOPPING;
    return detach(r);
}
}

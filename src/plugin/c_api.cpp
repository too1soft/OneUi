#include "runtime_internal.h"
#include <memory>

uint32_t oneui_plugin_abi_version(void) { return ONEUI_PLUGIN_ABI_V1; }
OneUiPluginResult oneui_plugin_runtime_create_v1(OneUiPluginRuntime **out) {
    if (!out) return ONEUI_PLUGIN_INVALID_ARGUMENT;
    *out = nullptr;
    try { *out = new OneUiPluginRuntime; return ONEUI_PLUGIN_OK; }
    catch (...) { return ONEUI_PLUGIN_FAILED; }
}
OneUiPluginResult oneui_plugin_runtime_register_v1(OneUiPluginRuntime *r, const OneUiPluginDescriptorV1 *d) {
    if (!r || !d) return ONEUI_PLUGIN_INVALID_ARGUMENT;
    const auto check = r->check();
    if (check != ONEUI_PLUGIN_OK) return check;
    return oneui::plugin::guard(*r, [&] { return oneui::plugin::registerPlugin(*r, *d); });
}
OneUiPluginResult oneui_plugin_runtime_start(OneUiPluginRuntime *r) {
    if (!r) return ONEUI_PLUGIN_INVALID_ARGUMENT;
    const auto check = r->check();
    if (check != ONEUI_PLUGIN_OK) return check;
    return oneui::plugin::guard(*r, [&] { return oneui::plugin::start(*r); });
}
OneUiPluginResult oneui_plugin_runtime_stop(OneUiPluginRuntime *r) {
    if (!r) return ONEUI_PLUGIN_INVALID_ARGUMENT;
    const auto check = r->check();
    if (check != ONEUI_PLUGIN_OK) return check;
    return oneui::plugin::guard(*r, [&] { return oneui::plugin::stop(*r); });
}
OneUiPluginResult oneui_plugin_runtime_destroy(OneUiPluginRuntime *r) {
    if (!r) return ONEUI_PLUGIN_INVALID_ARGUMENT;
    const auto check = r->check();
    if (check != ONEUI_PLUGIN_OK) return check;
    if (r->leases || (r->state != ONEUI_PLUGIN_REGISTERING && r->state != ONEUI_PLUGIN_STOPPED))
        return r->fail(ONEUI_PLUGIN_BUSY, "Stop the plugin runtime before destroying it");
    delete r;
    return ONEUI_PLUGIN_OK;
}
OneUiPluginResult oneui_plugin_runtime_state(OneUiPluginRuntime *r, OneUiPluginState *out) {
    if (!out) return ONEUI_PLUGIN_INVALID_ARGUMENT;
    *out = ONEUI_PLUGIN_STOPPED;
    if (!r) return ONEUI_PLUGIN_INVALID_ARGUMENT;
    const auto check = r->check(false);
    if (check != ONEUI_PLUGIN_OK) return check;
    *out = r->state;
    return ONEUI_PLUGIN_OK;
}
const char *oneui_plugin_runtime_error(OneUiPluginRuntime *r) {
    if (!r) return "Invalid plugin runtime";
    if (r->check(false) != ONEUI_PLUGIN_OK) return "Wrong plugin runtime thread";
    return r->error.data();
}
OneUiPluginResult oneui_plugin_runtime_acquire_v1(OneUiPluginRuntime *r, const char *id,
                                                uint32_t abi, OneUiPluginLease **out) {
    if (!out) return ONEUI_PLUGIN_INVALID_ARGUMENT;
    *out = nullptr;
    if (!r) return ONEUI_PLUGIN_INVALID_ARGUMENT;
    const auto check = r->check();
    if (check != ONEUI_PLUGIN_OK) return check;
    return oneui::plugin::guard(*r, [&] {
        if (r->state != ONEUI_PLUGIN_ACTIVE) return r->fail(ONEUI_PLUGIN_INVALID_STATE, "Plugin runtime is not active");
        if (!oneui::plugin::validId(id) || abi == 0) return ONEUI_PLUGIN_INVALID_ARGUMENT;
        auto found = r->services.find({id, abi});
        if (found == r->services.end()) return r->fail(ONEUI_PLUGIN_NOT_FOUND, "Service interface unavailable", id);
        if (r->leases >= 65536) return r->fail(ONEUI_PLUGIN_LIMIT, "Service lease count limit");
        auto lease = std::make_unique<OneUiPluginLease>(OneUiPluginLease{r, found->second.value});
        ++r->leases;
        *out = lease.release();
        return ONEUI_PLUGIN_OK;
    });
}
OneUiPluginResult oneui_plugin_lease_service_v1(OneUiPluginLease *lease, OneUiPluginServiceV1 *out) {
    if (!out) return ONEUI_PLUGIN_INVALID_ARGUMENT;
    *out = {};
    if (!lease) return ONEUI_PLUGIN_INVALID_ARGUMENT;
    const auto check = lease->runtime->check(false);
    if (check != ONEUI_PLUGIN_OK) return check;
    *out = lease->service;
    return ONEUI_PLUGIN_OK;
}
OneUiPluginResult oneui_plugin_lease_release(OneUiPluginLease *lease) {
    if (!lease) return ONEUI_PLUGIN_INVALID_ARGUMENT;
    const auto check = lease->runtime->check();
    if (check != ONEUI_PLUGIN_OK) return check;
    --lease->runtime->leases;
    delete lease;
    return ONEUI_PLUGIN_OK;
}

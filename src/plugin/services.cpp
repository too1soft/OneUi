#include "runtime_internal.h"
#include <algorithm>

namespace oneui::plugin {
OneUiPluginResult publish(void *token, const char *id, const OneUiPluginServiceV1 *value) {
    if (!token) return ONEUI_PLUGIN_INVALID_ARGUMENT;
    auto &e = *static_cast<Entry *>(token);
    auto &r = *e.runtime;
    const auto check = r.check(false);
    if (check != ONEUI_PLUGIN_OK) return check;
    return guard(r, [&] {
        if (r.state != ONEUI_PLUGIN_STARTING || r.attaching != &e)
            return r.fail(ONEUI_PLUGIN_INVALID_STATE, "Services can only be published during attach");
        if (!validId(id) || !value || value->struct_size != sizeof(*value) ||
            value->abi_version == 0 || !value->api)
            return r.fail(ONEUI_PLUGIN_INVALID_ARGUMENT, "Invalid service descriptor");
        if (r.services.size() >= 4096) return r.fail(ONEUI_PLUGIN_LIMIT, "Service count limit");
        if (!r.services.emplace(ServiceKey{id, value->abi_version}, Service{&e, *value}).second)
            return r.fail(ONEUI_PLUGIN_DUPLICATE, "Duplicate service interface", id);
        return ONEUI_PLUGIN_OK;
    });
}
OneUiPluginResult dependency(void *token, const char *id, uint32_t abi, OneUiPluginServiceV1 *out) {
    if (!out) return ONEUI_PLUGIN_INVALID_ARGUMENT;
    *out = {};
    if (!token) return ONEUI_PLUGIN_INVALID_ARGUMENT;
    auto &e = *static_cast<Entry *>(token);
    auto &r = *e.runtime;
    const auto check = r.check(false);
    if (check != ONEUI_PLUGIN_OK) return check;
    return guard(r, [&] {
        if (!e.attempted || (r.state != ONEUI_PLUGIN_STARTING && r.state != ONEUI_PLUGIN_ACTIVE &&
                             r.state != ONEUI_PLUGIN_STOPPING))
            return r.fail(ONEUI_PLUGIN_INVALID_STATE, "Plugin is not attached");
        if (!validId(id) || abi == 0) return ONEUI_PLUGIN_INVALID_ARGUMENT;
        const auto found = r.services.find({id, abi});
        if (found == r.services.end()) return r.fail(ONEUI_PLUGIN_NOT_FOUND, "Dependency service unavailable", id);
        const auto owner = found->second.owner;
        if (std::none_of(e.dependencies.begin(), e.dependencies.end(),
                         [&](const Dependency &d) { return d.id == owner->id; }))
            return r.fail(ONEUI_PLUGIN_NOT_FOUND, "Service requires a declared plugin dependency", id);
        *out = found->second.value;
        return ONEUI_PLUGIN_OK;
    });
}
}

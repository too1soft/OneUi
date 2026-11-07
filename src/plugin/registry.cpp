#include "runtime_internal.h"
#include <algorithm>
#include <cstdio>
#include <functional>
#include <tuple>

OneUiPluginResult OneUiPluginRuntime::check(bool mutation) const {
    if (owner != std::this_thread::get_id()) return ONEUI_PLUGIN_WRONG_THREAD;
    if (mutation && operating) return ONEUI_PLUGIN_REENTRANT;
    return ONEUI_PLUGIN_OK;
}
OneUiPluginResult OneUiPluginRuntime::fail(OneUiPluginResult result, const char *reason, const char *id) {
    std::snprintf(error.data(), error.size(), "%s%s%s", reason, *id ? ": " : "", id);
    return result;
}
namespace oneui::plugin {
bool validId(const char *id) {
    if (!id || *id < 'a' || *id > 'z') return false;
    for (size_t i = 0; i <= 128; ++i) {
        const char c = id[i];
        if (!c) return i <= 128;
        if (i == 128 || !((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                         c == '.' || c == '_' || c == '-')) return false;
    }
    return false;
}
bool less(OneUiPluginVersionV1 a, OneUiPluginVersionV1 b) {
    return std::tie(a.major, a.minor, a.patch) < std::tie(b.major, b.minor, b.patch);
}
bool zero(OneUiPluginVersionV1 v) { return v.major == 0 && v.minor == 0 && v.patch == 0; }
OneUiPluginResult registerPlugin(OneUiPluginRuntime &r, const OneUiPluginDescriptorV1 &d) {
    if (r.state != ONEUI_PLUGIN_REGISTERING)
        return r.fail(ONEUI_PLUGIN_INVALID_STATE, "Plugin registration is closed");
    if (d.struct_size != sizeof(d) || d.abi_version != ONEUI_PLUGIN_ABI_V1)
        return r.fail(ONEUI_PLUGIN_INCOMPATIBLE, "Plugin descriptor ABI mismatch");
    if (!validId(d.id) || !d.attach || !d.detach || d.dependency_count > 64 ||
        (d.dependency_count && !d.dependencies))
        return r.fail(ONEUI_PLUGIN_INVALID_ARGUMENT, "Invalid plugin descriptor");
    if (r.plugins.count(d.id)) return r.fail(ONEUI_PLUGIN_DUPLICATE, "Duplicate plugin", d.id);
    if (r.plugins.size() >= 256) return r.fail(ONEUI_PLUGIN_LIMIT, "Plugin count limit");
    Entry e;
    e.id = d.id; e.version = d.version; e.userData = d.user_data;
    e.attach = d.attach; e.detach = d.detach; e.runtime = &r;
    for (size_t i = 0; i < d.dependency_count; ++i) {
        const auto &dep = d.dependencies[i];
        if (!validId(dep.id) || (!zero(dep.before) && !less(dep.minimum, dep.before)))
            return r.fail(ONEUI_PLUGIN_INVALID_ARGUMENT, "Invalid dependency version range", d.id);
        if (std::any_of(e.dependencies.begin(), e.dependencies.end(),
                        [&](const Dependency &p) { return p.id == dep.id; }))
            return r.fail(ONEUI_PLUGIN_DUPLICATE, "Duplicate dependency", dep.id);
        e.dependencies.push_back({dep.id, dep.minimum, dep.before});
    }
    auto &stored = r.plugins.emplace(e.id, std::move(e)).first->second;
    stored.context = {sizeof(OneUiPluginContextV1), ONEUI_PLUGIN_ABI_V1, &stored, publish, dependency};
    return ONEUI_PLUGIN_OK;
}
OneUiPluginResult resolve(OneUiPluginRuntime &r) {
    std::map<std::string, int> marks;
    std::vector<Entry *> ordered;
    ordered.reserve(r.plugins.size());
    std::function<OneUiPluginResult(Entry &)> visit = [&](Entry &e) {
        auto &mark = marks[e.id];
        if (mark == 2) return ONEUI_PLUGIN_OK;
        if (mark == 1) return r.fail(ONEUI_PLUGIN_DEPENDENCY_CYCLE, "Dependency cycle", e.id.c_str());
        mark = 1;
        for (const auto &dep : e.dependencies) {
            const auto found = r.plugins.find(dep.id);
            if (found == r.plugins.end()) return r.fail(ONEUI_PLUGIN_NOT_FOUND, "Missing dependency", dep.id.c_str());
            const auto v = found->second.version;
            if (less(v, dep.minimum) || (!zero(dep.before) && !less(v, dep.before)))
                return r.fail(ONEUI_PLUGIN_INCOMPATIBLE, "Dependency version mismatch", dep.id.c_str());
            const auto result = visit(found->second);
            if (result != ONEUI_PLUGIN_OK) return result;
        }
        mark = 2;
        ordered.push_back(&e);
        return ONEUI_PLUGIN_OK;
    };
    for (auto &item : r.plugins) {
        const auto result = visit(item.second);
        if (result != ONEUI_PLUGIN_OK) return result;
    }
    r.order.swap(ordered);
    return ONEUI_PLUGIN_OK;
}
}

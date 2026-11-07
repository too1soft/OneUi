#pragma once
#include "oneui/plugin/c_api.h"
#include <array>
#include <map>
#include <string>
#include <thread>
#include <vector>

namespace oneui::plugin {
struct Dependency {
    std::string id;
    OneUiPluginVersionV1 minimum{}, before{};
};
struct Entry {
    std::string id;
    OneUiPluginVersionV1 version{};
    std::vector<Dependency> dependencies;
    void *userData = nullptr, *instance = nullptr;
    OneUiPluginResult (*attach)(const OneUiPluginContextV1 *, void *, void **) = nullptr;
    OneUiPluginResult (*detach)(void *) = nullptr;
    OneUiPluginContextV1 context{};
    OneUiPluginRuntime *runtime = nullptr;
    bool attempted = false;
};
struct Service {
    Entry *owner;
    OneUiPluginServiceV1 value;
};
using ServiceKey = std::pair<std::string, uint32_t>;
bool validId(const char *id);
bool less(OneUiPluginVersionV1 a, OneUiPluginVersionV1 b);
bool zero(OneUiPluginVersionV1 v);
OneUiPluginResult publish(void *, const char *, const OneUiPluginServiceV1 *);
OneUiPluginResult dependency(void *, const char *, uint32_t, OneUiPluginServiceV1 *);
OneUiPluginResult registerPlugin(OneUiPluginRuntime &, const OneUiPluginDescriptorV1 &);
OneUiPluginResult resolve(OneUiPluginRuntime &);
OneUiPluginResult start(OneUiPluginRuntime &);
OneUiPluginResult stop(OneUiPluginRuntime &);
}

struct OneUiPluginRuntime {
    const std::thread::id owner = std::this_thread::get_id();
    std::map<std::string, oneui::plugin::Entry> plugins;
    std::map<oneui::plugin::ServiceKey, oneui::plugin::Service> services;
    std::vector<oneui::plugin::Entry *> order;
    OneUiPluginState state = ONEUI_PLUGIN_REGISTERING;
    oneui::plugin::Entry *attaching = nullptr;
    size_t leases = 0;
    bool operating = false;
    std::array<char, 1024> error{};
    OneUiPluginResult check(bool mutation = true) const;
    OneUiPluginResult fail(OneUiPluginResult result, const char *reason, const char *id = "");
};
struct OneUiPluginLease {
    OneUiPluginRuntime *runtime;
    OneUiPluginServiceV1 service;
};

namespace oneui::plugin {
struct Operation {
    OneUiPluginRuntime &runtime;
    explicit Operation(OneUiPluginRuntime &r) : runtime(r) { r.operating = true; }
    ~Operation() { runtime.operating = false; }
};
template <class F> OneUiPluginResult guard(OneUiPluginRuntime &runtime, F f) noexcept {
    try { return f(); }
    catch (...) { return runtime.fail(ONEUI_PLUGIN_FAILED, "Plugin operation raised an exception"); }
}
}

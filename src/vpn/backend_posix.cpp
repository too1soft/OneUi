#include "oneui/vpn/backend.h"
#include <dlfcn.h>
#include <mutex>

namespace oneui::vpn {
namespace {
std::mutex mutex;
std::string loadedName;
Backend loaded;
void *module = nullptr;
bool validModule = false;
}
const Backend *loadBackend(const std::string &library) {
    if (library.empty() || library.size() > 128) return nullptr;
    for (const char c : library)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-')) return nullptr;
    std::lock_guard<std::mutex> lock(mutex);
    if (module) return validModule && loadedName == library ? &loaded : nullptr;
    void *candidate = dlopen(("lib" + library + ".so").c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!candidate) return nullptr;
    auto version = reinterpret_cast<uint32_t (*)()>(dlsym(candidate, "oneui_vpn_go_abi_version"));
    loaded.create = reinterpret_cast<Backend::Create>(dlsym(candidate, "oneui_vpn_go_host_create"));
    loaded.inject = reinterpret_cast<Backend::Inject>(dlsym(candidate, "oneui_vpn_go_packet_inject"));
    loaded.receive = reinterpret_cast<Backend::Receive>(dlsym(candidate, "oneui_vpn_go_packet_receive"));
    loaded.close = reinterpret_cast<Backend::Close>(dlsym(candidate, "oneui_vpn_go_host_close"));
    loaded.changed = reinterpret_cast<Backend::Changed>(dlsym(candidate, "oneui_vpn_go_network_changed"));
    module = candidate; loadedName = library;
    validModule = version && version() == 1 && loaded.create && loaded.inject && loaded.receive && loaded.close && loaded.changed;
    return validModule ? &loaded : nullptr;
}
const Backend *currentBackend() {
    std::lock_guard<std::mutex> lock(mutex);
    return validModule ? &loaded : nullptr;
}
}

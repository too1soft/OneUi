#include "oneui/mobile/native_module_posix.h"
#include "oneui/mobile/native_module.h"
#include <dlfcn.h>
#include <limits>
#include <mutex>
#include <unordered_map>

namespace oneui::mobile {
namespace {
std::mutex mutex;
uint64_t nextHandle = 0;
std::unordered_map<uint64_t, OneUiNativeModuleCallV1> functions;
std::unordered_map<std::string, void *> modules;
bool valid(const std::string &text, bool library) {
    if (text.empty() || text.size() > 128) return false;
    for (const char c : text)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || (library && c == '-'))) return false;
    return true;
}
void wipe(std::vector<unsigned char> &bytes) {
    volatile unsigned char *data = bytes.data();
    for (size_t i = 0; i < bytes.size(); ++i) data[i] = 0;
    bytes.clear();
}
void *symbol(const std::string &library, const std::string &entry) {
    if (modules.find(library) == modules.end() && modules.size() >= 16) return nullptr;
    auto &module = modules[library];
    if (!module) module = dlopen(("lib" + library + ".so").c_str(), RTLD_NOW | RTLD_LOCAL);
    return module ? dlsym(module, entry.c_str()) : nullptr;
}
}
void *NativeModulePosix::resolve(const std::string &library, const std::string &entry) {
    if (!valid(library, true) || !valid(entry, false)) return nullptr;
    std::lock_guard<std::mutex> lock(mutex); return symbol(library, entry);
}
uint64_t NativeModulePosix::open(const std::string &library, const std::string &symbol) {
    if (!valid(library, true) || !valid(symbol, false)) return 0;
    std::lock_guard<std::mutex> lock(mutex);
    if (functions.size() >= 256 || nextHandle == std::numeric_limits<uint64_t>::max()) return 0;
    auto call = reinterpret_cast<OneUiNativeModuleCallV1>(::oneui::mobile::symbol(library, symbol));
    if (!call) return 0;
    functions.emplace(++nextHandle, call); return nextHandle;
}
int NativeModulePosix::call(uint64_t handle, int command, std::vector<unsigned char> request, std::vector<unsigned char> &reply) {
    wipe(reply);
    if (request.size() > 1024 * 1024) { wipe(request); return -2; }
    OneUiNativeModuleCallV1 call;
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto found = functions.find(handle);
        if (found == functions.end()) { wipe(request); return -1; }
        call = found->second;
    }
    std::vector<unsigned char> output(1024 * 1024);
    const int count = call(command, request.data(), static_cast<int>(request.size()), output.data(), static_cast<int>(output.size()));
    wipe(request);
    if (count < 0 || count > static_cast<int>(output.size())) { wipe(output); return count < 0 ? count : -4; }
    reply.assign(output.begin(), output.begin() + count); wipe(output); return 0;
}
void NativeModulePosix::release(uint64_t handle) {
    std::lock_guard<std::mutex> lock(mutex); functions.erase(handle);
}
}

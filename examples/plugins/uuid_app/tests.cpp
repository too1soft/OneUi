#include "oneui/plugins/uuid.h"
#include "oneui_generated_plugins.h"
#include <array>
#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
void require(bool value, const char *message) {
    if (!value) throw std::runtime_error(message);
}
void okay(OneUiPluginResult result) { require(result == ONEUI_PLUGIN_OK, "Plugin operation failed"); }
void canonical(const char *value) {
    require(std::strlen(value) == 36, "UUID length");
    for (size_t i = 0; i != 36; ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23)
            require(value[i] == '-', "UUID separators");
        else
            require((value[i] >= '0' && value[i] <= '9') ||
                    (value[i] >= 'a' && value[i] <= 'f'), "UUID lowercase hexadecimal");
    }
    require(value[14] == '4', "UUID version");
    require(std::strchr("89ab", value[19]), "UUID variant");
}
struct Session {
    OneUiPluginRuntime *runtime = nullptr;
    OneUiPluginLease *lease = nullptr;
    OneUiPluginServiceV1 service{};
    Session() {
        okay(oneui_plugin_runtime_create_v1(&runtime));
        okay(register_uuid_test_plugins(runtime));
        okay(oneui_plugin_runtime_start(runtime));
        okay(oneui_plugin_runtime_acquire_v1(runtime, ONEUI_UUID_SERVICE_V1, 1, &lease));
        okay(oneui_plugin_lease_service_v1(lease, &service));
    }
    ~Session() {
        if (lease) oneui_plugin_lease_release(lease);
        if (runtime) {
            oneui_plugin_runtime_stop(runtime);
            oneui_plugin_runtime_destroy(runtime);
        }
    }
    const OneUiUuidApiV1 &api() const { return *static_cast<const OneUiUuidApiV1 *>(service.api); }
};
void run() {
    Session first, second;
    require(first.service.instance != second.service.instance, "Runtime instance isolation");
    require(first.api().struct_size >= sizeof(OneUiUuidApiV1) && first.api().abi_version == 1, "Service ABI");
    std::set<std::string> seen;
    for (int i = 0; i != 4096; ++i) {
        std::array<char, ONEUI_UUID_BUFFER_SIZE + 1> value{};
        value.back() = '!';
        okay(first.api().generate(first.service.instance, value.data(), ONEUI_UUID_BUFFER_SIZE));
        canonical(value.data());
        require(value.back() == '!', "Output overrun");
        require(seen.insert(value.data()).second, "Duplicate UUID in native sample");
    }
    char small[4] = {'x', 'a', 'b', '!'};
    require(first.api().generate(first.service.instance, small, 3) == ONEUI_PLUGIN_LIMIT,
            "Undersized buffer must fail");
    require(small[0] == 0 && small[1] == 'a' && small[3] == '!', "Failure writes outside contract");
    require(first.api().generate(first.service.instance, nullptr, 37) == ONEUI_PLUGIN_INVALID_ARGUMENT,
            "Null output");
    char text[ONEUI_UUID_BUFFER_SIZE] = "not empty";
    require(first.api().generate(nullptr, text, sizeof(text)) == ONEUI_PLUGIN_INVALID_ARGUMENT && !text[0],
            "Null instance must clear output");
    OneUiPluginResult crossThread = ONEUI_PLUGIN_OK;
    std::thread worker([&] { crossThread = first.api().generate(first.service.instance, text, sizeof(text)); });
    worker.join();
    require(crossThread == ONEUI_PLUGIN_WRONG_THREAD && !text[0], "Owner thread enforcement");
    require(oneui_plugin_runtime_stop(first.runtime) == ONEUI_PLUGIN_BUSY, "Lease must prevent shutdown");
    okay(first.api().generate(first.service.instance, text, sizeof(text)));
    canonical(text);
    okay(second.api().generate(second.service.instance, text, sizeof(text)));
    canonical(text);
    okay(oneui_plugin_lease_release(first.lease));
    first.lease = nullptr;
    okay(oneui_plugin_runtime_stop(first.runtime));
    okay(oneui_plugin_runtime_stop(first.runtime));
    okay(oneui_plugin_runtime_destroy(first.runtime));
    first.runtime = nullptr;
    std::cout << "PASS 4096 native UUIDs, canonical format, buffer errors, owner thread, runtime isolation and leases\n";
}
}
int main() {
    try { run(); return 0; }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}

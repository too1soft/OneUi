#include "oneui/plugin/c_api.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
void check(bool value, const char *message) {
    if (!value) throw std::runtime_error(message);
}
void ok(OneUiPluginResult result) { check(result == ONEUI_PLUGIN_OK, "Expected OK"); }
struct Fixture {
    std::string id, service;
    std::vector<std::string> *events = nullptr;
    const OneUiPluginContextV1 *context = nullptr;
    OneUiPluginRuntime *runtime = nullptr;
    const char *requiredService = nullptr;
    OneUiPluginResult attachResult = ONEUI_PLUGIN_OK, cleanupResult = ONEUI_PLUGIN_OK;
    OneUiPluginResult dependencyResult = ONEUI_PLUGIN_OK, reentrantResult = ONEUI_PLUGIN_OK;
    bool throws = false;
    int calls = 0, cleanups = 0;
};
const int serviceApi = 42;
OneUiPluginResult attach(const OneUiPluginContextV1 *ctx, void *data, void **instance) {
    auto &f = *static_cast<Fixture *>(data);
    *instance = &f;
    ++f.calls;
    f.context = ctx;
    if (f.events) f.events->push_back("+" + f.id);
    if (f.runtime) f.reentrantResult = oneui_plugin_runtime_stop(f.runtime);
    if (f.requiredService) {
        OneUiPluginServiceV1 found{};
        f.dependencyResult = ctx->dependency(ctx->token, f.requiredService, 1, &found);
        if (f.dependencyResult != ONEUI_PLUGIN_OK) return f.dependencyResult;
        check(found.api == &serviceApi, "Dependency table is unchanged");
    }
    if (!f.service.empty()) {
        OneUiPluginServiceV1 value{sizeof(value), 1, &serviceApi, &f};
        const auto result = ctx->publish(ctx->token, f.service.c_str(), &value);
        if (result != ONEUI_PLUGIN_OK) return result;
    }
    if (f.throws) throw std::runtime_error("foreign callback");
    return f.attachResult;
}
OneUiPluginResult detach(void *instance) {
    auto &f = *static_cast<Fixture *>(instance);
    ++f.cleanups;
    if (f.events) f.events->push_back("-" + f.id);
    return f.cleanupResult;
}
OneUiPluginDescriptorV1 descriptor(Fixture &f, const OneUiPluginDependencyV1 *deps = nullptr, size_t count = 0) {
    return {sizeof(OneUiPluginDescriptorV1), 1, f.id.c_str(), {1, 2, 3}, deps, count, &f, attach, detach};
}
OneUiPluginRuntime *create() {
    OneUiPluginRuntime *r = nullptr;
    ok(oneui_plugin_runtime_create_v1(&r));
    check(r != nullptr, "Runtime output");
    return r;
}
OneUiPluginState state(OneUiPluginRuntime *r) {
    OneUiPluginState result;
    ok(oneui_plugin_runtime_state(r, &result));
    return result;
}
void dependencyOrderAndLeases() {
    auto *r = create();
    std::vector<std::string> events;
    Fixture provider{"z.provider", "shared.counter", &events}, consumer{"a.consumer", "", &events};
    consumer.requiredService = "shared.counter";
    const OneUiPluginDependencyV1 dep{"z.provider", {1, 0, 0}, {2, 0, 0}};
    auto a = descriptor(consumer, &dep, 1), b = descriptor(provider);
    ok(oneui_plugin_runtime_register_v1(r, &a));
    ok(oneui_plugin_runtime_register_v1(r, &b));
    ok(oneui_plugin_runtime_start(r));
    check(events == std::vector<std::string>{"+z.provider", "+a.consumer"}, "Topological attach order");
    check(state(r) == ONEUI_PLUGIN_ACTIVE, "Active state");
    check(oneui_plugin_runtime_register_v1(r, &b) == ONEUI_PLUGIN_INVALID_STATE, "Registration sealed");
    OneUiPluginLease *lease = nullptr;
    ok(oneui_plugin_runtime_acquire_v1(r, "shared.counter", 1, &lease));
    OneUiPluginServiceV1 value{};
    ok(oneui_plugin_lease_service_v1(lease, &value));
    check(value.api == &serviceApi && value.instance == &provider, "Service ownership");
    check(oneui_plugin_runtime_stop(r) == ONEUI_PLUGIN_BUSY, "Leases block cleanup");
    check(state(r) == ONEUI_PLUGIN_ACTIVE && provider.cleanups == 0, "Busy stop is non-destructive");
    check(oneui_plugin_runtime_destroy(r) == ONEUI_PLUGIN_BUSY, "Live runtime cannot be deleted");
    ok(oneui_plugin_lease_release(lease));
    ok(oneui_plugin_runtime_stop(r));
    check(events == std::vector<std::string>{"+z.provider", "+a.consumer", "-a.consumer", "-z.provider"}, "Reverse cleanup");
    check(oneui_plugin_runtime_start(r) == ONEUI_PLUGIN_INVALID_STATE, "No restart after attachment");
    ok(oneui_plugin_runtime_stop(r));
    ok(oneui_plugin_runtime_destroy(r));
}
void preflight() {
    for (int scenario = 0; scenario < 3; ++scenario) {
        auto *r = create();
        Fixture a{"a.plugin"}, b{"b.plugin"};
        OneUiPluginDependencyV1 ab{"b.plugin", {1, 0, 0}, {2, 0, 0}};
        OneUiPluginDependencyV1 ba{"a.plugin", {1, 0, 0}, {2, 0, 0}};
        if (scenario == 1) ab.minimum = {1, 2, 4};
        auto da = descriptor(a, &ab, 1), db = descriptor(b, scenario == 2 ? &ba : nullptr, scenario == 2 ? 1 : 0);
        ok(oneui_plugin_runtime_register_v1(r, &da));
        if (scenario) ok(oneui_plugin_runtime_register_v1(r, &db));
        const auto expected = scenario == 0 ? ONEUI_PLUGIN_NOT_FOUND :
                              scenario == 1 ? ONEUI_PLUGIN_INCOMPATIBLE : ONEUI_PLUGIN_DEPENDENCY_CYCLE;
        check(oneui_plugin_runtime_start(r) == expected, "Dependency graph rejection");
        check(a.calls == 0 && b.calls == 0, "Preflight has no plugin side effects");
        check(state(r) == ONEUI_PLUGIN_REGISTERING, "Can correct preflight dependencies");
        ok(oneui_plugin_runtime_destroy(r));
    }
}
void rollbackAndCleanupRetry() {
    auto *r = create();
    Fixture a{"a.provider", "shared.counter"}, b{"b.consumer"}, c{"c.untouched"};
    b.attachResult = ONEUI_PLUGIN_FAILED;
    b.cleanupResult = ONEUI_PLUGIN_BUSY;
    const OneUiPluginDependencyV1 dep{"a.provider", {1, 0, 0}, {2, 0, 0}};
    auto da = descriptor(a), db = descriptor(b, &dep, 1), dc = descriptor(c);
    ok(oneui_plugin_runtime_register_v1(r, &da));
    ok(oneui_plugin_runtime_register_v1(r, &db));
    ok(oneui_plugin_runtime_register_v1(r, &dc));
    check(oneui_plugin_runtime_start(r) == ONEUI_PLUGIN_BUSY, "Failed cleanup propagated");
    check(state(r) == ONEUI_PLUGIN_STOPPING, "Partial ownership retained");
    check(a.cleanups == 0 && c.calls == 0, "Dependencies retained and untouched plugins skipped");
    check(std::string(oneui_plugin_runtime_error(r)).find("b.consumer") != std::string::npos, "Cleanup identifies plugin");
    OneUiPluginLease *lease = reinterpret_cast<OneUiPluginLease *>(1);
    check(oneui_plugin_runtime_acquire_v1(r, "shared.counter", 1, &lease) == ONEUI_PLUGIN_INVALID_STATE && !lease,
          "No new consumers during incomplete cleanup");
    check(oneui_plugin_runtime_destroy(r) == ONEUI_PLUGIN_BUSY, "Partial runtime retained");
    b.cleanupResult = ONEUI_PLUGIN_OK;
    ok(oneui_plugin_runtime_stop(r));
    check(b.cleanups == 2 && a.cleanups == 1, "Only incomplete cleanup is retried");
    ok(oneui_plugin_runtime_destroy(r));
}
void duplicateServicesAndExceptions() {
    for (bool throws : {false, true}) {
        auto *r = create();
        Fixture a{"a.provider", "shared.counter"}, b{"b.provider", "shared.counter"};
        b.throws = throws;
        if (throws) b.service.clear();
        auto da = descriptor(a), db = descriptor(b);
        ok(oneui_plugin_runtime_register_v1(r, &da));
        ok(oneui_plugin_runtime_register_v1(r, &db));
        check(oneui_plugin_runtime_start(r) == (throws ? ONEUI_PLUGIN_FAILED : ONEUI_PLUGIN_DUPLICATE),
              "Duplicate interfaces and callback exceptions fail attachment");
        check(a.cleanups == 1 && b.cleanups == 1 && state(r) == ONEUI_PLUGIN_STOPPED, "Complete rollback");
        ok(oneui_plugin_runtime_destroy(r));
    }
}
void threadAndReentrancy() {
    auto *r = create();
    Fixture f{"thread.plugin", "shared.counter"};
    f.runtime = r;
    auto d = descriptor(f);
    ok(oneui_plugin_runtime_register_v1(r, &d));
    ok(oneui_plugin_runtime_start(r));
    check(f.reentrantResult == ONEUI_PLUGIN_REENTRANT, "Lifecycle callback cannot stop runtime recursively");
    OneUiPluginLease *lease = nullptr;
    ok(oneui_plugin_runtime_acquire_v1(r, "shared.counter", 1, &lease));
    std::thread other([&] {
        OneUiPluginServiceV1 value{};
        OneUiPluginState s;
        check(oneui_plugin_runtime_stop(r) == ONEUI_PLUGIN_WRONG_THREAD, "Owner thread stop");
        check(oneui_plugin_runtime_state(r, &s) == ONEUI_PLUGIN_WRONG_THREAD, "Owner thread state");
        check(oneui_plugin_lease_service_v1(lease, &value) == ONEUI_PLUGIN_WRONG_THREAD, "Owner thread service");
        check(oneui_plugin_lease_release(lease) == ONEUI_PLUGIN_WRONG_THREAD, "Owner thread release");
    });
    other.join();
    ok(oneui_plugin_lease_release(lease));
    OneUiPluginServiceV1 value{sizeof(value), 1, &serviceApi, &f};
    check(f.context->publish(f.context->token, "late.service", &value) == ONEUI_PLUGIN_INVALID_STATE,
          "Service registration frozen after attach");
    ok(oneui_plugin_runtime_stop(r));
    ok(oneui_plugin_runtime_destroy(r));
}
void undeclaredDependenciesAndIsolation() {
    auto *r = create();
    Fixture a{"a.provider", "shared.counter"}, b{"b.consumer"};
    b.requiredService = "shared.counter";
    auto da = descriptor(a), db = descriptor(b);
    ok(oneui_plugin_runtime_register_v1(r, &da));
    ok(oneui_plugin_runtime_register_v1(r, &db));
    check(oneui_plugin_runtime_start(r) == ONEUI_PLUGIN_NOT_FOUND, "Undeclared dependency rejected");
    ok(oneui_plugin_runtime_destroy(r));
    auto *one = create(), *two = create();
    Fixture f{"isolated.plugin", "shared.counter"}, g{"isolated.plugin", "shared.counter"};
    auto df = descriptor(f), dg = descriptor(g);
    ok(oneui_plugin_runtime_register_v1(one, &df));
    ok(oneui_plugin_runtime_register_v1(two, &dg));
    ok(oneui_plugin_runtime_start(one));
    ok(oneui_plugin_runtime_start(two));
    ok(oneui_plugin_runtime_stop(one));
    check(g.cleanups == 0 && state(two) == ONEUI_PLUGIN_ACTIVE, "Application instances isolated");
    ok(oneui_plugin_runtime_stop(two));
    ok(oneui_plugin_runtime_destroy(one));
    ok(oneui_plugin_runtime_destroy(two));
}
void descriptorOwnershipAndValidation() {
    auto *r = create();
    Fixture f{"copied.plugin"};
    auto d = descriptor(f);
    d.abi_version = 99;
    check(oneui_plugin_runtime_register_v1(r, &d) == ONEUI_PLUGIN_INCOMPATIBLE, "Descriptor ABI checked");
    d.abi_version = 1;
    d.id = "../escape";
    check(oneui_plugin_runtime_register_v1(r, &d) == ONEUI_PLUGIN_INVALID_ARGUMENT, "Invalid identifier");
    std::string name = "copied.plugin";
    d.id = name.c_str();
    ok(oneui_plugin_runtime_register_v1(r, &d));
    check(oneui_plugin_runtime_register_v1(r, &d) == ONEUI_PLUGIN_DUPLICATE, "Duplicate ID rejected");
    name.assign("changed.name");
    ok(oneui_plugin_runtime_start(r));
    check(f.calls == 1, "Descriptor strings copied");
    ok(oneui_plugin_runtime_stop(r));
    ok(oneui_plugin_runtime_destroy(r));
    check(oneui_plugin_runtime_create_v1(nullptr) == ONEUI_PLUGIN_INVALID_ARGUMENT, "NULL output rejected");
}
}
int main() {
    try {
        dependencyOrderAndLeases(); preflight(); rollbackAndCleanupRetry();
        duplicateServicesAndExceptions(); threadAndReentrancy();
        undeclaredDependenciesAndIsolation(); descriptorOwnershipAndValidation();
        std::cout << "7 plugin lifecycle suites passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

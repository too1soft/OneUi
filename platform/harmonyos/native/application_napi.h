#pragma once
#include "napi_support.h"
#include "platform/harmonyos/application_surface.h"
#include <set>
#include <thread>

namespace oneui::harmony::application {
struct Host {
    napi_env owner;
    std::thread::id thread = std::this_thread::get_id();
    mobile::harmony::ApplicationSurface surface;
    std::set<uint64_t> requests;
    Host(napi_env env, const mobile::ApplicationHost::Factory &factory) : owner(env), surface(factory) {}
};
std::shared_ptr<Host> host(napi_env env, napi_value value);
double real(napi_env env, napi_value value);
void registerApplication(napi_env env, napi_value exports);
void registerInput(napi_env env, napi_value exports);
}

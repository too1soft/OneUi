#pragma once
#if defined(ONEUI_NODE_TEST)
#include <node_api.h>
#else
#include <napi/native_api.h>
#endif
#include "oneui/mobile/native_service_queue.h"
#include <array>
#include <functional>
#include <string>
#include <stdexcept>
#include <vector>

namespace oneui::harmony {
using Reply = mobile::NativeServiceReply;
void check(napi_status status);
napi_value safe(napi_env env, const std::function<napi_value()> &call);
std::vector<napi_value> args(napi_env env, napi_callback_info info, size_t count);
std::string text(napi_env env, napi_value value, size_t maximum = 128);
uint64_t handle(napi_env env, napi_value value);
int integer(napi_env env, napi_value value);
std::vector<unsigned char> bytes(napi_env env, napi_value value, size_t maximum = 1024 * 1024 + 4096);
napi_value number(napi_env env, int value);
napi_value string(napi_env env, const std::string &value);
napi_value buffer(napi_env env, const std::string &value);
napi_value boolean(napi_env env, bool value);
napi_value undefined(napi_env env);
void property(napi_env env, napi_value object, const char *name, napi_value value);
napi_value async(napi_env env, std::function<Reply()> work, bool engine = false);
void registerModules(napi_env env, napi_value exports);
void registerPorts(napi_env env, napi_value exports);
void registerSessions(napi_env env, napi_value exports);
void registerBlobFiles(napi_env env, napi_value exports);
void registerNetwork(napi_env env, napi_value exports);
void registerChannels(napi_env env, napi_value exports);
}

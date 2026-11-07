#include "napi_support.h"
#include <atomic>
#include <charconv>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace oneui::harmony {
void check(napi_status status) { if (status != napi_ok) throw std::runtime_error("N-API operation rejected"); }
napi_value safe(napi_env env, const std::function<napi_value()> &call) {
    try { return call(); }
    catch (...) { napi_throw_error(env, nullptr, "OneUI native argument or state error"); return nullptr; }
}
std::vector<napi_value> args(napi_env env, napi_callback_info info, size_t expected) {
    std::vector<napi_value> values(expected); size_t count = expected;
    check(napi_get_cb_info(env, info, &count, values.data(), nullptr, nullptr));
    if (count != expected) throw std::runtime_error("Argument count");
    return values;
}
std::string text(napi_env env, napi_value value, size_t maximum) {
    size_t length = 0; check(napi_get_value_string_utf8(env, value, nullptr, 0, &length));
    if (length > maximum) throw std::runtime_error("String bound");
    std::string result(length + 1, '\0');
    check(napi_get_value_string_utf8(env, value, result.data(), result.size(), &length)); result.resize(length);
    if (result.find('\0') != std::string::npos) throw std::runtime_error("Embedded NUL");
    return result;
}
uint64_t handle(napi_env env, napi_value value) {
    const auto input = text(env, value, 20); uint64_t result = 0;
    auto parsed = std::from_chars(input.data(), input.data() + input.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != input.data() + input.size() || !result) throw std::runtime_error("Handle");
    return result;
}
int integer(napi_env env, napi_value value) {
    double result; check(napi_get_value_double(env, value, &result));
    if (!std::isfinite(result) || result != std::floor(result) || result < std::numeric_limits<int>::min() || result > std::numeric_limits<int>::max())
        throw std::runtime_error("Integer");
    return static_cast<int>(result);
}
std::vector<unsigned char> bytes(napi_env env, napi_value value, size_t maximum) {
    napi_typedarray_type type; size_t length, offset; void *data; napi_value owner;
    check(napi_get_typedarray_info(env, value, &type, &length, &data, &owner, &offset));
    if (type != napi_uint8_array || length > maximum) throw std::runtime_error("Byte array");
    if (!length) return {};
    auto *start = static_cast<unsigned char *>(data); return {start, start + length};
}
napi_value number(napi_env env, int value) { napi_value result; check(napi_create_int32(env, value, &result)); return result; }
napi_value string(napi_env env, const std::string &value) { napi_value result; check(napi_create_string_utf8(env, value.data(), value.size(), &result)); return result; }
napi_value buffer(napi_env env, const std::string &value) {
    napi_value owner, result; void *destination;
    check(napi_create_arraybuffer(env, value.size(), &destination, &owner));
    if (!value.empty()) std::memcpy(destination, value.data(), value.size());
    check(napi_create_typedarray(env, napi_uint8_array, value.size(), owner, 0, &result)); return result;
}
napi_value boolean(napi_env env, bool value) { napi_value result; check(napi_get_boolean(env, value, &result)); return result; }
napi_value undefined(napi_env env) { napi_value result; check(napi_get_undefined(env, &result)); return result; }
void property(napi_env env, napi_value object, const char *name, napi_value value) { check(napi_set_named_property(env, object, name, value)); }
namespace {
std::atomic<unsigned> jobs{0};
std::atomic<unsigned> engineJobs{0};
struct Job { napi_async_work work; napi_deferred deferred; std::function<Reply()> run; Reply reply; bool engine = false; };
void execute(napi_env, void *data) {
    auto &job = *static_cast<Job *>(data);
    try { job.reply = job.run(); } catch (...) { job.reply.code = -4; }
}
void complete(napi_env env, napi_status status, void *data) {
    std::unique_ptr<Job> job(static_cast<Job *>(data));
    try {
        if (status == napi_ok && job->reply.code == 0) check(napi_resolve_deferred(env, job->deferred, buffer(env, job->reply.payload)));
        else {
            napi_value error; check(napi_create_error(env, nullptr, string(env, "OneUI native operation failed"), &error));
            property(env, error, "code", number(env, status == napi_ok ? job->reply.code : -1));
            check(napi_reject_deferred(env, job->deferred, error));
        }
    } catch (...) { /* Environment teardown must not throw across N-API. */ }
    volatile char *output = job->reply.payload.empty() ? nullptr : &job->reply.payload[0];
    for (size_t i = 0; i < job->reply.payload.size(); ++i) output[i] = 0;
    napi_delete_async_work(env, job->work); --jobs; if (job->engine) --engineJobs;
}
}
napi_value async(napi_env env, std::function<Reply()> work, bool engine) {
    auto job = std::make_unique<Job>();
    // Leave N-API workers available for asynchronous OS callbacks needed by Go.
    if (engine && engineJobs.fetch_add(1) >= 2) { --engineJobs; throw std::runtime_error("Engine work capacity"); }
    if (jobs.fetch_add(1) >= 16) { --jobs; if (engine) --engineJobs; throw std::runtime_error("Native work capacity"); }
    job->engine = engine; job->run = std::move(work); napi_value promise;
    try {
        check(napi_create_promise(env, &job->deferred, &promise));
        check(napi_create_async_work(env, nullptr, string(env, "OneUI native service"), execute, complete, job.get(), &job->work));
        auto status = napi_queue_async_work(env, job->work);
        if (status != napi_ok) { napi_delete_async_work(env, job->work); check(status); }
    } catch (...) { --jobs; if (engine) --engineJobs; throw; }
    job.release(); return promise;
}
}

#include "native_port.h"
#include <cstring>

namespace oneui::harmony {
void wipe(std::string &text) {
    volatile char *data = text.empty() ? nullptr : &text[0];
    for (size_t i = 0; i < text.size(); ++i) data[i] = 0;
    text.clear();
}
namespace {
Reply invoke(uintptr_t context, int operation, std::string data) {
    return reinterpret_cast<NativePort *>(context)->services.invoke(operation, std::move(data), std::chrono::seconds(10));
}
int apply(uintptr_t context, const char *json, int length) {
    if (!json || length < 1 || length > 1024 * 1024) return -2;
    return invoke(context, 1, {json, static_cast<size_t>(length)}).code;
}
int protect(uintptr_t context, int fd) {
    return fd < 0 ? -2 : invoke(context, 2, std::to_string(fd)).code;
}
size_t keyLength(const char *key) {
    if (!key) return 0;
    const auto length = strnlen(key, 129);
    return length > 128 ? 0 : length;
}
int readSecret(uintptr_t context, const char *key, void *buffer, int capacity) {
    const auto length = keyLength(key);
    if (!length || !buffer || capacity < 1) return -2;
    auto reply = invoke(context, 3, {key, length});
    const int result = reply.code != 0 ? reply.code :
        (reply.payload.size() > static_cast<size_t>(capacity) ? -2 : static_cast<int>(reply.payload.size()));
    if (result > 0) std::memcpy(buffer, reply.payload.data(), static_cast<size_t>(result));
    wipe(reply.payload); return result;
}
int writeSecret(uintptr_t context, const char *key, const void *bytes, int length) {
    const auto keySize = keyLength(key);
    if (!keySize || !bytes || length < 1 || length > 1024 * 1024) return -2;
    std::string payload; payload.push_back(static_cast<char>(keySize)); payload.push_back(static_cast<char>(keySize >> 8));
    payload.append(key, keySize); payload.append(static_cast<const char *>(bytes), static_cast<size_t>(length));
    return invoke(context, 4, std::move(payload)).code;
}
int network(uintptr_t context, void *buffer, int capacity) {
    if (!buffer || capacity < 1) return -2;
    auto reply = invoke(context, 5, {});
    const int result = reply.code != 0 ? reply.code :
        (reply.payload.size() > static_cast<size_t>(capacity) ? -2 : static_cast<int>(reply.payload.size()));
    if (result > 0) std::memcpy(buffer, reply.payload.data(), static_cast<size_t>(result));
    wipe(reply.payload); return result;
}
}
OneUiVpnNativeCallbacks NativePort::callbacks(NativePort &port) {
    return {sizeof(OneUiVpnNativeCallbacks), 1, reinterpret_cast<uintptr_t>(&port), apply, protect, readSecret, writeSecret, network};
}
}

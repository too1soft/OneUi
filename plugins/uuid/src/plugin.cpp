#include "oneui/plugins/uuid.h"
#include <array>
#include <cstring>
#include <new>
#include <thread>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#elif defined(__APPLE__)
#include <Security/SecRandom.h>
#else
#error OneUI UUID currently has Windows and macOS backends only
#endif

namespace {
struct Generator {
    const std::thread::id owner = std::this_thread::get_id();
};

bool systemRandom(std::array<unsigned char, 16> &bytes) noexcept {
#if defined(_WIN32)
    return BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(bytes.size()),
                           BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0;
#else
    return SecRandomCopyBytes(kSecRandomDefault, bytes.size(), bytes.data()) == errSecSuccess;
#endif
}

OneUiPluginResult generate(void *opaque, char *out, size_t capacity) noexcept {
    if (out && capacity)
        out[0] = '\0';
    if (!opaque || !out)
        return ONEUI_PLUGIN_INVALID_ARGUMENT;
    if (static_cast<Generator *>(opaque)->owner != std::this_thread::get_id())
        return ONEUI_PLUGIN_WRONG_THREAD;
    if (capacity < ONEUI_UUID_BUFFER_SIZE)
        return ONEUI_PLUGIN_LIMIT;
    std::array<unsigned char, 16> bytes{};
    if (!systemRandom(bytes))
        return ONEUI_PLUGIN_FAILED;
    bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0f) | 0x40);
    bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3f) | 0x80);
    constexpr char hex[] = "0123456789abcdef";
    char result[ONEUI_UUID_BUFFER_SIZE]{};
    size_t position = 0;
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10)
            result[position++] = '-';
        result[position++] = hex[bytes[i] >> 4];
        result[position++] = hex[bytes[i] & 0x0f];
    }
    std::memcpy(out, result, sizeof(result));
    return ONEUI_PLUGIN_OK;
}

const OneUiUuidApiV1 api{sizeof(api), 1, generate};

OneUiPluginResult attach(const OneUiPluginContextV1 *context, void *, void **out) noexcept {
    if (out)
        *out = nullptr;
    if (!context || !out || context->struct_size < sizeof(*context) ||
        context->abi_version != ONEUI_PLUGIN_ABI_V1 || !context->publish)
        return ONEUI_PLUGIN_INVALID_ARGUMENT;
    *out = new (std::nothrow) Generator;
    if (!*out)
        return ONEUI_PLUGIN_FAILED;
    const OneUiPluginServiceV1 service{sizeof(service), 1, &api, *out};
    return context->publish(context->token, ONEUI_UUID_SERVICE_V1, &service);
}

OneUiPluginResult detach(void *opaque) noexcept {
    auto *instance = static_cast<Generator *>(opaque);
    if (instance && instance->owner != std::this_thread::get_id())
        return ONEUI_PLUGIN_WRONG_THREAD;
    delete instance;
    return ONEUI_PLUGIN_OK;
}

const OneUiPluginDescriptorV1 descriptor{
    sizeof(descriptor), 1, "oneui.uuid", {0, 1, 0}, nullptr, 0, nullptr, attach, detach};
} // namespace

const OneUiPluginDescriptorV1 *oneui_uuid_plugin_descriptor_v1(void) { return &descriptor; }

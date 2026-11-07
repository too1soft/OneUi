#include <jni.h>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#include "native_bridge.h"
#include "native_backend.h"

namespace {
class Environment {
public:
    JNIEnv *env = nullptr;
    explicit Environment(JavaVM *vm) : vm_(vm) {
        const auto status = vm_->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
        if (status == JNI_EDETACHED) {
            attached_ = vm_->AttachCurrentThread(&env, nullptr) == JNI_OK;
            if (!attached_) env = nullptr;
        } else if (status != JNI_OK) env = nullptr;
        if (env && env->PushLocalFrame(16) != JNI_OK) {
            env->ExceptionClear(); env = nullptr;
        } else framed_ = env != nullptr;
    }
    ~Environment() {
        if (framed_) env->PopLocalFrame(nullptr);
        if (attached_) vm_->DetachCurrentThread();
    }
private:
    JavaVM *vm_;
    bool attached_ = false, framed_ = false;
};
struct Context {
    JavaVM *vm = nullptr;
    jobject services = nullptr;
    jmethodID apply = nullptr, protect = nullptr, read = nullptr, write = nullptr, network = nullptr;
    ~Context() {
        if (!vm || !services) return;
        Environment scope(vm);
        if (scope.env) scope.env->DeleteGlobalRef(services);
    }
};
std::mutex contextsMutex;
std::unordered_map<uint64_t, std::unique_ptr<Context>> contexts;

bool exception(JNIEnv *env) {
    if (!env->ExceptionCheck()) return false;
    env->ExceptionClear(); // Stable numeric errors; never log credential exceptions.
    return true;
}
jstring utf8(JNIEnv *env, const char *data, int length) {
    auto bytes = env->NewByteArray(length);
    if (!bytes) return nullptr;
    env->SetByteArrayRegion(bytes, 0, length, reinterpret_cast<const jbyte *>(data));
    auto klass = env->FindClass("java/lang/String");
    if (!klass || exception(env)) return nullptr;
    auto constructor = env->GetMethodID(klass, "<init>", "([BLjava/lang/String;)V");
    if (!constructor || exception(env)) return nullptr;
    auto encoding = env->NewStringUTF("UTF-8");
    if (!encoding) return nullptr;
    return static_cast<jstring>(env->NewObject(klass, constructor, bytes, encoding));
}
void wipe(JNIEnv *env, jbyteArray bytes) {
    const jsize length = env->GetArrayLength(bytes);
    const jbyte zeros[4096] = {};
    for (jsize i = 0; i < length; i += 4096)
        env->SetByteArrayRegion(bytes, i, (length - i < 4096 ? length - i : 4096), zeros);
    exception(env);
}
int apply(uintptr_t value, const char *json, int length) {
    auto *context = reinterpret_cast<Context *>(value);
    Environment scope(context->vm);
    auto *env = scope.env;
    if (!env) return -1;
    auto text = utf8(env, json, length);
    const bool invalidText = exception(env);
    if (!text || invalidText) return -1;
    const int result = env->CallIntMethod(context->services, context->apply, text);
    return exception(env) ? -1 : result;
}
int protect(uintptr_t value, int descriptor) {
    auto *context = reinterpret_cast<Context *>(value);
    Environment scope(context->vm);
    if (!scope.env) return -1;
    const auto result = scope.env->CallBooleanMethod(context->services, context->protect, descriptor);
    return exception(scope.env) || result != JNI_TRUE ? -1 : 0;
}
int read(uintptr_t value, const char *key, void *output, int capacity) {
    auto *context = reinterpret_cast<Context *>(value);
    Environment scope(context->vm);
    auto *env = scope.env;
    if (!env) return -1;
    auto name = utf8(env, key, static_cast<int>(std::strlen(key)));
    if (!name) { exception(env); return -1; }
    auto bytes = static_cast<jbyteArray>(env->CallObjectMethod(context->services, context->read, name));
    if (exception(env)) return -1;
    if (!bytes) return 0;
    const int length = env->GetArrayLength(bytes);
    if (length > capacity) { wipe(env, bytes); return -1; }
    env->GetByteArrayRegion(bytes, 0, length, static_cast<jbyte *>(output));
    const bool failed = exception(env);
    wipe(env, bytes);
    return failed ? -1 : length;
}
int write(uintptr_t value, const char *key, const void *data, int length) {
    auto *context = reinterpret_cast<Context *>(value);
    Environment scope(context->vm);
    auto *env = scope.env;
    if (!env) return -1;
    auto name = utf8(env, key, static_cast<int>(std::strlen(key)));
    auto bytes = env->NewByteArray(length);
    if (!name || !bytes) { exception(env); return -1; }
    env->SetByteArrayRegion(bytes, 0, length, static_cast<const jbyte *>(data));
    if (exception(env)) { wipe(env, bytes); return -1; }
    env->CallVoidMethod(context->services, context->write, name, bytes);
    const bool failed = exception(env);
    wipe(env, bytes);
    return failed ? -1 : 0;
}
int networkState(uintptr_t value, void *output, int capacity) {
    auto *context = reinterpret_cast<Context *>(value);
    Environment scope(context->vm);
    auto *env = scope.env;
    if (!env) return -1;
    auto bytes = static_cast<jbyteArray>(env->CallObjectMethod(context->services, context->network));
    if (exception(env) || !bytes) return -1;
    const int length = env->GetArrayLength(bytes);
    if (length < 1 || length > capacity) return -1;
    env->GetByteArrayRegion(bytes, 0, length, static_cast<jbyte *>(output));
    return exception(env) ? -1 : length;
}
}

extern "C" JNIEXPORT jlong JNICALL
Java_net_too1_oneui_vpn_OneUiNativePacketPort_nativeCreate(JNIEnv *env, jclass, jbyteArray library, jbyteArray label, jint mtu, jint flags, jobject services) {
    if (!library || !label || !services) return 0;
    const auto libraryLength = env->GetArrayLength(library);
    if (libraryLength < 1 || libraryLength > 128) return 0;
    std::string libraryName(static_cast<size_t>(libraryLength), '\0');
    env->GetByteArrayRegion(library, 0, libraryLength, reinterpret_cast<jbyte *>(libraryName.data()));
    if (exception(env)) return 0;
    const auto *backend = oneui::vpn::android::loadBackend(libraryName);
    if (!backend) return 0;
    const auto length = env->GetArrayLength(label);
    if (length < 1 || length > 128) return 0;
    std::vector<char> text(static_cast<size_t>(length) + 1, '\0');
    env->GetByteArrayRegion(label, 0, length, reinterpret_cast<jbyte *>(text.data()));
    if (exception(env)) return 0;
    for (int i = 0; i < length; ++i) if (text[i] == '\0') return 0;
    auto context = std::make_unique<Context>();
    if (env->GetJavaVM(&context->vm) != JNI_OK) return 0;
    context->services = env->NewGlobalRef(services);
    const auto klass = env->GetObjectClass(services);
    const bool invalidClass = exception(env);
    if (!context->services || !klass || invalidClass) return 0;
    context->apply = env->GetMethodID(klass, "applyNetwork", "(Ljava/lang/String;)I");
    context->protect = env->GetMethodID(klass, "protectSocket", "(I)Z");
    context->read = env->GetMethodID(klass, "secureRead", "(Ljava/lang/String;)[B");
    context->write = env->GetMethodID(klass, "secureWrite", "(Ljava/lang/String;[B)V");
    context->network = env->GetMethodID(klass, "networkState", "()[B");
    const bool invalidMethods = exception(env);
    if (!context->apply || !context->protect || !context->read || !context->write || !context->network || invalidMethods) return 0;
    OneUiVpnNativeCallbacks callbacks{sizeof(OneUiVpnNativeCallbacks), 1, reinterpret_cast<uintptr_t>(context.get()), apply, protect, read, write, networkState};
    const auto host = backend->create(text.data(), mtu, flags, &callbacks);
    if (!host) return 0;
    std::lock_guard<std::mutex> lock(contextsMutex);
    contexts.emplace(host, std::move(context));
    return static_cast<jlong>(host);
}
extern "C" JNIEXPORT jint JNICALL
Java_net_too1_oneui_vpn_OneUiNativePacketPort_nativeInject(JNIEnv *env, jclass, jlong host, jbyteArray packet, jint length, jint timeout) {
    if (!packet || length < 1 || length > 65535 || length > env->GetArrayLength(packet)) return -2;
    std::vector<jbyte> bytes(static_cast<size_t>(length));
    env->GetByteArrayRegion(packet, 0, length, bytes.data());
    if (exception(env)) return -2;
    const auto *backend = oneui::vpn::android::currentBackend();
    return backend ? backend->inject(static_cast<uint64_t>(host), bytes.data(), length, timeout) : -1;
}
extern "C" JNIEXPORT jint JNICALL
Java_net_too1_oneui_vpn_OneUiNativePacketPort_nativeReceive(JNIEnv *env, jclass, jlong host, jbyteArray buffer, jint timeout) {
    if (!buffer || env->GetArrayLength(buffer) < 65535) return -2;
    std::vector<jbyte> bytes(65535);
    const auto *backend = oneui::vpn::android::currentBackend();
    if (!backend) return -1;
    const int result = backend->receive(static_cast<uint64_t>(host), bytes.data(), 65535, timeout);
    if (result > 0 && result <= 65535) {
        env->SetByteArrayRegion(buffer, 0, result, bytes.data());
        if (exception(env)) return -4;
    }
    return result;
}
extern "C" JNIEXPORT jint JNICALL
Java_net_too1_oneui_vpn_OneUiNativePacketPort_nativeDestroy(JNIEnv *, jclass, jlong host) {
    std::lock_guard<std::mutex> lock(contextsMutex);
    const auto found = contexts.find(static_cast<uint64_t>(host));
    if (found == contexts.end()) return -1;
    const auto *backend = oneui::vpn::android::currentBackend();
    if (!backend) return -1;
    const int result = backend->close(static_cast<uint64_t>(host));
    if (result == 0) contexts.erase(found);
    return result;
}
extern "C" JNIEXPORT jint JNICALL
Java_net_too1_oneui_vpn_OneUiNativePacketPort_nativeNetworkChanged(JNIEnv *, jclass, jlong host) {
    const auto *backend = oneui::vpn::android::currentBackend();
    return backend ? backend->changed(static_cast<uint64_t>(host)) : -1;
}

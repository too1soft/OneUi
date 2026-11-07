#include <jni.h>
#include <string>
#include <vector>
#include "oneui/mobile/native_module_posix.h"

namespace {
std::string text(JNIEnv *env, jbyteArray value) {
    if (!value || env->GetArrayLength(value) < 1 || env->GetArrayLength(value) > 128) return {};
    std::string result(static_cast<size_t>(env->GetArrayLength(value)), '\0');
    env->GetByteArrayRegion(value, 0, static_cast<jsize>(result.size()), reinterpret_cast<jbyte *>(&result[0]));
    if (env->ExceptionCheck()) { env->ExceptionClear(); return {}; }
    return result;
}
}
extern "C" JNIEXPORT jlong JNICALL
Java_net_too1_oneui_nativebridge_NativeModule_open(JNIEnv *env, jclass, jbyteArray library, jbyteArray symbol) {
    return static_cast<jlong>(oneui::mobile::NativeModulePosix::open(text(env, library), text(env, symbol)));
}
extern "C" JNIEXPORT jbyteArray JNICALL
Java_net_too1_oneui_nativebridge_NativeModule_invoke(JNIEnv *env, jclass, jlong handle, jint command, jbyteArray request) {
    if (!request) return nullptr;
    const auto length = env->GetArrayLength(request);
    if (length < 0 || length > 1024 * 1024) return nullptr;
    std::vector<unsigned char> bytes(static_cast<size_t>(length)), output;
    env->GetByteArrayRegion(request, 0, length, reinterpret_cast<jbyte *>(bytes.data()));
    if (env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }
    if (oneui::mobile::NativeModulePosix::call(static_cast<uint64_t>(handle), command, std::move(bytes), output) != 0) return nullptr;
    auto result = env->NewByteArray(static_cast<jsize>(output.size()));
    if (result) env->SetByteArrayRegion(result, 0, static_cast<jsize>(output.size()), reinterpret_cast<const jbyte *>(output.data()));
    volatile unsigned char *data = output.data();
    for (size_t i = 0; i < output.size(); ++i) data[i] = 0;
    return result;
}
extern "C" JNIEXPORT void JNICALL
Java_net_too1_oneui_nativebridge_NativeModule_release(JNIEnv *, jclass, jlong handle) {
    oneui::mobile::NativeModulePosix::release(static_cast<uint64_t>(handle));
}

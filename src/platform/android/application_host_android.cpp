#include "platform/android/application_host_android.h"
#include <android/native_window_jni.h>
#include <string>
#include <vector>
#include <utility>
#include "oneui/canvas.h"

namespace oneui::mobile::android {
ApplicationSurface *applicationSurface(JNIEnv *env, jlong handle) {
    auto *value = reinterpret_cast<ApplicationSurface *>(handle);
    if (!value || value->runtime().checkThread() != ONEUI_MOBILE_OK) {
        env->ThrowNew(env->FindClass("java/lang/IllegalStateException"), "OneUI host requires its owner thread");
        return nullptr;
    }
    return value;
}
}
using namespace oneui::mobile::android;
#define JNI_METHOD(name) Java_net_too1_oneui_mobile_NativeApplication_##name
// One symbol forces this JNI object and its text bridge into a consumer link.
extern "C" void oneui_android_text_link_anchor();
extern "C" void oneui_android_application_link_anchor() { oneui_android_text_link_anchor(); }
extern "C" JNIEXPORT jlong JNICALL JNI_METHOD(create)(JNIEnv *env, jclass) {
    try { return reinterpret_cast<jlong>(new ApplicationSurface); }
    catch (...) {
        env->ThrowNew(env->FindClass("java/lang/IllegalStateException"), "OneUI application initialization failed");
        return 0;
    }
}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(destroy)(JNIEnv *env, jclass, jlong handle) {
    if (auto *host = applicationSurface(env, handle)) { host->suspend(); delete host; }
}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(lifecycle)(JNIEnv *env, jclass, jlong handle, jint state) {
    if (auto *host = applicationSurface(env, handle)) host->error = host->core.lifecycle(state);
}
extern "C" JNIEXPORT jboolean JNICALL JNI_METHOD(attach)(JNIEnv *env, jclass, jlong handle, jobject surface, jint width, jint height, jfloat scale) {
    auto *host = applicationSurface(env, handle);
    if (!host || !surface) return false;
    // A size change (notably adjustResize for the IME) replaces the EGL
    // surface without detaching the application or invalidating its editor.
    // Actual surface loss still goes through detach()/suspend().
    host->core.cancelPointers();
    host->width = width;
    host->height = height;
    host->scale = scale;
    host->error = host->layout();
    if (host->error != ONEUI_MOBILE_OK) { host->core.surface(false); return false; }
    if (!host->surface.attach(ANativeWindow_fromSurface(env, surface), width, height)) { host->core.surface(false); host->error = ONEUI_MOBILE_FAILED; return false; }
    host->error = host->core.surface(true);
    return host->error == ONEUI_MOBILE_OK;
}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(editorInset)(JNIEnv *env, jclass, jlong handle, jint pixels) {
    auto *host = applicationSurface(env, handle);
    if (!host || host->width <= 0 || host->height <= 0 || pixels < 0 || host->editorInset == pixels) return;
    host->editorInset = pixels;
    host->error = host->layout();
    host->core.widgets().pumpInput();
}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(detach)(JNIEnv *env, jclass, jlong handle) {
    if (auto *host = applicationSurface(env, handle)) host->suspend();
}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(pointer)(JNIEnv *env, jclass, jlong handle, jint id, jint phase, jint kind, jfloat x, jfloat y, jfloat pressure, jlong time) {
    if (auto *host = applicationSurface(env, handle)) host->error = host->core.pointer({id, phase, kind, x, y, pressure, uint64_t(time), 0});
}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(cancel)(JNIEnv *env, jclass, jlong handle) {
    if (auto *host = applicationSurface(env, handle)) host->core.cancelPointers();
}
extern "C" JNIEXPORT jboolean JNICALL JNI_METHOD(back)(JNIEnv *env, jclass, jlong handle) {
    auto *host = applicationSurface(env, handle);
    return host && host->core.back();
}
extern "C" JNIEXPORT jboolean JNICALL JNI_METHOD(needsFrame)(JNIEnv *env, jclass, jlong handle) {
    auto *host = applicationSurface(env, handle);
    if (!host) return false;
    host->core.flush();
    OneUiMobileStateV1 state{};
    host->runtime().state(state);
    return state.needs_frame != 0;
}
extern "C" JNIEXPORT jint JNICALL JNI_METHOD(wakeDelayMillis)(JNIEnv *env, jclass, jlong handle) {
    auto *host = applicationSurface(env, handle);
    return host ? host->core.services().wakeDelayMillis() : -1;
}
extern "C" JNIEXPORT jboolean JNICALL JNI_METHOD(draw)(JNIEnv *env, jclass, jlong handle, jlong time) {
    auto *host = applicationSurface(env, handle);
    if (!host) return false;
    try {
        std::unique_ptr<oneui::Canvas> canvas;
        host->error = host->core.paint([&] { canvas = host->surface.begin(host->scale); return canvas.get(); }, uint64_t(time));
        canvas.reset();
        if (host->error == ONEUI_MOBILE_EMPTY) { host->error = 0; return true; }
        if (host->error == ONEUI_MOBILE_OK && host->surface.present()) { ++host->frames; return true; }
    } catch (...) { }
    host->suspend(); host->error = ONEUI_MOBILE_FAILED;
    return false;
}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(observeAppearance)(JNIEnv *env, jclass, jlong handle, jint theme, jboolean available) {
    if (auto *host = applicationSurface(env, handle)) { host->core.application().appearance().observe(theme, available); host->core.syncAppearance(); }
}
extern "C" JNIEXPORT jlongArray JNICALL JNI_METHOD(appearance)(JNIEnv *env, jclass, jlong handle) {
    auto *host = applicationSurface(env, handle);
    if (!host) return nullptr;
    OneUiMobileAppearanceStateV1 state{};
    host->core.application().appearance().state(state);
    jlong values[] = {jlong(state.revision), jlong(state.applied_revision), jlong(state.status_argb), jlong(state.navigation_argb), state.status_icons, state.navigation_icons, state.effective_theme};
    auto result = env->NewLongArray(7);
    if (result) env->SetLongArrayRegion(result, 0, 7, values);
    return result;
}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(acknowledgeAppearance)(JNIEnv *env, jclass, jlong handle, jlong revision) {
    if (auto *host = applicationSurface(env, handle)) host->core.application().appearance().acknowledge(uint64_t(revision));
}
extern "C" JNIEXPORT jobjectArray JNICALL JNI_METHOD(requests)(JNIEnv *env, jclass, jlong handle) {
    auto *host = applicationSurface(env, handle);
    if (!host) return nullptr;
    auto klass = env->FindClass("net/too1/oneui/mobile/NativeRequest");
    if (!klass) return nullptr;
    auto constructor = env->GetMethodID(klass, "<init>", "(JLjava/lang/String;[B)V");
    if (!constructor) return nullptr;
    std::vector<oneui::mobile::ApplicationRequest> requests;
    oneui::mobile::ApplicationRequest request;
    while (host->core.services().take(request)) requests.push_back(std::move(request));
    auto result = env->NewObjectArray(jsize(requests.size()), klass, nullptr);
    if (!result) return nullptr;
    for (size_t i = 0; i < requests.size(); ++i) {
        const auto &entry = requests[i];
        auto action = env->NewStringUTF(entry.action.c_str());
        auto data = env->NewByteArray(jsize(entry.payload.size()));
        if (!action || !data) return nullptr;
        env->SetByteArrayRegion(data, 0, jsize(entry.payload.size()), reinterpret_cast<const jbyte *>(entry.payload.data()));
        auto value = env->NewObject(klass, constructor, jlong(entry.id), action, data);
        if (!value) return nullptr;
        env->SetObjectArrayElement(result, jsize(i), value);
        env->DeleteLocalRef(value); env->DeleteLocalRef(action); env->DeleteLocalRef(data);
    }
    return result;
}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(reply)(JNIEnv *env, jclass, jlong handle, jlong id, jint code, jbyteArray data) {
    auto *host = applicationSurface(env, handle);
    if (!host || !data) return;
    const auto length = env->GetArrayLength(data);
    if (length < 0 || length > 1024 * 1024) return;
    std::string payload(static_cast<size_t>(length), '\0');
    env->GetByteArrayRegion(data, 0, length, reinterpret_cast<jbyte *>(payload.data()));
    if (!env->ExceptionCheck()) host->core.reply(uint64_t(id), code, payload);
}

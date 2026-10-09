#include "egl_surface.h"
#include "oneui/mobile/runtime.h"
#include <android/log.h>
#include <android/native_window_jni.h>
#include <jni.h>
#include <cstdio>
#include <new>
#include <string>

namespace {
struct Host {
    oneui::mobile::Runtime runtime;
    EglSurface surface;
    int width = 0, height = 0, error = 0;
    float x = 100, y = 100, scale = 1;
    bool pressed = false;
    unsigned frames = 0, events = 0, cancels = 0;
    std::string renderer = "unavailable";
    void consume() {
        OneUiMobilePointerV1 event{};
        while (runtime.pollPointer(event) == ONEUI_MOBILE_OK) {
            ++events;
            if (event.phase == ONEUI_MOBILE_CANCEL) { pressed = false; ++cancels; }
            else if (event.primary) {
                x = event.x; y = event.y;
                pressed = event.phase != ONEUI_MOBILE_UP;
            }
            runtime.invalidate();
        }
    }
};
Host* host(JNIEnv* env, jlong value) {
    auto* result = reinterpret_cast<Host*>(value);
    if (!result || result->runtime.checkThread() != ONEUI_MOBILE_OK) {
        env->ThrowNew(env->FindClass("java/lang/IllegalStateException"), "Mobile host requires its owning UI thread");
        return nullptr;
    }
    return result;
}
}

#define JNI_METHOD(name) Java_net_too1_oneui_mobileprobe_NativeHost_##name
extern "C" JNIEXPORT jlong JNICALL JNI_METHOD(create)(JNIEnv* env, jclass) {
    auto* h = new (std::nothrow) Host;
    if (!h) env->ThrowNew(env->FindClass("java/lang/OutOfMemoryError"), "Mobile host allocation failed");
    return reinterpret_cast<jlong>(h);
}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(destroy)(JNIEnv* env, jclass, jlong value) {
    if (auto* h = host(env, value)) { h->runtime.setLifecycle(ONEUI_MOBILE_DESTROYED); delete h; }
}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(lifecycle)(JNIEnv* env, jclass, jlong value, jint state) {
    if (auto* h = host(env, value)) { h->error = h->runtime.setLifecycle(state); h->consume(); }
}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(detach)(JNIEnv* env, jclass, jlong value) {
    if (auto* h = host(env, value)) {
        h->runtime.setSurface(false); h->consume(); h->surface.reset();
    }
}
extern "C" JNIEXPORT jboolean JNICALL JNI_METHOD(attach)(JNIEnv* env, jclass, jlong value,
    jobject surface, jint width, jint height, jfloat scale) {
    auto* h = host(env, value); if (!h) return false;
    h->runtime.setSurface(false); h->consume(); h->surface.reset();
    h->error = h->runtime.setViewport({float(width), float(height), scale, 0, 0, 0, 0, 0});
    if (h->error != ONEUI_MOBILE_OK || !surface) return false;
    h->width = width; h->height = height; h->scale = scale;
    if (!h->surface.attach(ANativeWindow_fromSurface(env, surface))) { h->error = ONEUI_MOBILE_FAILED; return false; }
    h->renderer = h->surface.renderer();
    h->runtime.setSurface(true);
    __android_log_print(ANDROID_LOG_INFO, "OneUIMobile", "EGL ready: %s", h->renderer.c_str());
    return true;
}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(pointer)(JNIEnv* env, jclass, jlong value,
    jint id, jint phase, jint kind, jfloat x, jfloat y, jfloat pressure, jlong time) {
    if (auto* h = host(env, value)) {
        h->error = h->runtime.pushPointer({id, phase, kind, x, y, pressure, static_cast<uint64_t>(time), 0});
        if (h->error != ONEUI_MOBILE_OK) h->runtime.cancelPointers();
        h->consume();
    }
}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(cancel)(JNIEnv* env, jclass, jlong value) {
    if (auto* h = host(env, value)) { h->runtime.cancelPointers(); h->consume(); }
}
extern "C" JNIEXPORT jboolean JNICALL JNI_METHOD(needsFrame)(JNIEnv* env, jclass, jlong value) {
    auto* h = host(env, value); if (!h) return false;
    OneUiMobileStateV1 state{}; h->runtime.state(state); return state.needs_frame != 0;
}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(draw)(JNIEnv* env, jclass, jlong value, jlong time) {
    auto* h = host(env, value); if (!h) return;
    OneUiMobileFrameV1 frame{};
    const auto result = h->runtime.takeFrame(static_cast<uint64_t>(time), frame);
    if (result == ONEUI_MOBILE_EMPTY) return;
    if (result != ONEUI_MOBILE_OK) { h->error = result; return; }
    if (h->surface.draw(h->width, h->height, h->x, h->y, h->scale, h->pressed)) ++h->frames;
    else {
        h->error = ONEUI_MOBILE_FAILED;
        h->runtime.setSurface(false); h->consume(); h->surface.reset();
        __android_log_print(ANDROID_LOG_ERROR, "OneUIMobile", "EGL presentation failed; surface recreation required");
    }
}
extern "C" JNIEXPORT jstring JNICALL JNI_METHOD(status)(JNIEnv* env, jclass, jlong value) {
    auto* h = host(env, value); if (!h) return nullptr;
    OneUiMobileStateV1 state{}; h->runtime.state(state);
    char text[1024];
    std::snprintf(text, sizeof(text), "frames=%u  events=%u  cancels=%u\nlifecycle=%d  surface=%llu  touches=%u  pending=%u\nerror=%d  EGL: %.240s",
        h->frames, h->events, h->cancels, state.lifecycle,
        static_cast<unsigned long long>(state.surface_generation), state.active_pointers,
        state.needs_frame, h->error, h->renderer.c_str());
    __android_log_print(ANDROID_LOG_INFO, "OneUIMobile", "%s", text);
    return env->NewStringUTF(text);
}

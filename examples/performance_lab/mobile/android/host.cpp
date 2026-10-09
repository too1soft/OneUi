#include "mobile/workbench.hpp"
#include "oneui/mobile/widget_host.h"
#include "platform/android/skia_surface_android.h"
#include <android/log.h>
#include <android/native_window_jni.h>
#include <jni.h>
#include <sstream>

namespace {
using namespace oneui;
struct Host {
    mobile::WidgetHost widgets;
    mobile::SkiaSurface surface;
    lab::mobile::Workbench workbench;
    float scale = 1;
    unsigned frames = 0, events = 0;
    int error = 0;
    Host() {
        widgets.setTouchScrolling(true);
        widgets.setContent(workbench.widget());
    }
    mobile::Runtime &runtime() { return widgets.runtime(); }
    void activate() {
        OneUiMobileStateV1 state{};
        runtime().state(state);
        workbench.setActive(state.lifecycle == ONEUI_MOBILE_ACTIVE && state.surface_available);
    }
    void commands() {
        if (!workbench.hasPending())
            return;
        runtime().cancelPointers();
        widgets.pumpInput();
        workbench.flush();
    }
    void suspend() {
        workbench.setActive(false);
        runtime().setSurface(false);
        widgets.pumpInput();
        surface.reset();
    }
};
Host *host(JNIEnv *env, jlong value) {
    auto *h = reinterpret_cast<Host *>(value);
    if (!h || h->runtime().checkThread() != ONEUI_MOBILE_OK) {
        env->ThrowNew(env->FindClass("java/lang/IllegalStateException"),
                      "OneUI host requires its UI thread");
        return nullptr;
    }
    return h;
}
void fail(JNIEnv *env, const char *message) {
    env->ThrowNew(env->FindClass("java/lang/IllegalStateException"), message);
}
void controls(std::ostringstream &out, const std::shared_ptr<Widget> &widget, int &buttons,
              int &sliders) {
    const auto rect = widget->frame();
    const bool button = dynamic_cast<Button *>(widget.get()) != nullptr;
    const auto *slider = dynamic_cast<Slider *>(widget.get());
    if (button || slider) {
        out << ' ' << (button ? "button" : "slider") << (button ? buttons++ : sliders++) << '=';
        if (slider)
            out << slider->value() << ',';
        out << rect.x << ',' << rect.y << ',' << rect.width << ',' << rect.height;
    }
    if (auto grid = dynamic_cast<Grid *>(widget.get())) {
        out << " grid=" << rect.x << ',' << rect.y << ',' << rect.width << ',' << rect.height;
        float bottom = rect.y;
        for (const auto &child : grid->children())
            bottom = std::max(bottom, child->frame().y + child->frame().height);
        out << " gridBottom=" << bottom;
    }
    if (auto view = std::dynamic_pointer_cast<View>(widget))
        for (const auto &child : view->children())
            controls(out, child, buttons, sliders);
}
} // namespace
#define JNI_METHOD(name) Java_net_too1_oneui_lab_NativeHost_##name
extern "C" JNIEXPORT jlong JNICALL JNI_METHOD(create)(JNIEnv *env, jclass) {
    try {
        return reinterpret_cast<jlong>(new Host);
    } catch (const std::exception &e) {
        fail(env, e.what());
        return 0;
    }
}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(destroy)(JNIEnv *env, jclass, jlong value) {
    if (auto *h = host(env, value)) {
        h->widgets.setContent({});
        h->workbench.setActive(false);
        h->runtime().setLifecycle(ONEUI_MOBILE_DESTROYED);
        delete h;
    }
}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(lifecycle)(JNIEnv *env, jclass, jlong value,
                                                        jint state) {
    if (auto *h = host(env, value)) {
        h->error = h->runtime().setLifecycle(state);
        h->widgets.pumpInput();
        h->activate();
    }
}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(detach)(JNIEnv *env, jclass, jlong value) {
    if (auto *h = host(env, value))
        h->suspend();
}
extern "C" JNIEXPORT jboolean JNICALL JNI_METHOD(attach)(JNIEnv *env, jclass, jlong value,
                                                         jobject surface, jint width, jint height,
                                                         jfloat scale) {
    auto *h = host(env, value);
    if (!h)
        return false;
    h->suspend();
    h->error = h->runtime().setViewport({float(width), float(height), scale, 0, 0, 0, 0, 0});
    if (h->error || !surface)
        return false;
    h->scale = scale;
    h->workbench.viewport(float(width) / scale);
    if (!h->surface.attach(ANativeWindow_fromSurface(env, surface), width, height)) {
        h->error = ONEUI_MOBILE_FAILED;
        return false;
    }
    h->runtime().setSurface(true);
    h->activate();
    __android_log_print(ANDROID_LOG_INFO, "OneUILab", "Skia Ganesh: %s",
                        h->surface.renderer().c_str());
    return true;
}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(pointer)(JNIEnv *env, jclass, jlong value, jint id,
                                                      jint phase, jint kind, jfloat x, jfloat y,
                                                      jfloat pressure, jlong time) {
    if (auto *h = host(env, value)) {
        try {
            h->error =
                h->runtime().pushPointer({id, phase, kind, x, y, pressure, uint64_t(time), 0});
            if (h->error)
                h->runtime().cancelPointers();
            h->widgets.pumpInput();
            h->commands();
            ++h->events;
        } catch (const std::exception &e) {
            fail(env, e.what());
        }
    }
}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(cancel)(JNIEnv *env, jclass, jlong value) {
    if (auto *h = host(env, value)) {
        h->runtime().cancelPointers();
        h->widgets.pumpInput();
    }
}
extern "C" JNIEXPORT jboolean JNICALL JNI_METHOD(back)(JNIEnv *env, jclass, jlong value) {
    auto *h = host(env, value);
    if (!h)
        return false;
    h->runtime().cancelPointers();
    h->widgets.pumpInput();
    return h->workbench.back();
}
extern "C" JNIEXPORT jboolean JNICALL JNI_METHOD(needsFrame)(JNIEnv *env, jclass, jlong value) {
    auto *h = host(env, value);
    if (!h)
        return false;
    OneUiMobileStateV1 state{};
    h->runtime().state(state);
    return state.needs_frame != 0;
}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(draw)(JNIEnv *env, jclass, jlong value, jlong time) {
    auto *h = host(env, value);
    if (!h)
        return;
    try {
        auto canvas = h->surface.begin(h->scale);
        if (!canvas) {
            h->error = ONEUI_MOBILE_FAILED;
            h->suspend();
            return;
        }
        h->error = h->widgets.paint(*canvas, uint64_t(time));
        canvas.reset();
        if (h->error == ONEUI_MOBILE_OK && h->surface.present())
            ++h->frames;
        else {
            h->error = ONEUI_MOBILE_FAILED;
            h->suspend();
        }
    } catch (const std::exception &e) {
        h->suspend();
        fail(env, e.what());
    }
}
extern "C" JNIEXPORT jstring JNICALL JNI_METHOD(status)(JNIEnv *env, jclass, jlong value) {
    auto *h = host(env, value);
    if (!h)
        return nullptr;
    OneUiMobileStateV1 state{};
    h->runtime().state(state);
    auto scroll = std::dynamic_pointer_cast<ScrollView>(h->workbench.page()->widget());
    std::ostringstream out;
    out << "frames=" << h->frames << " events=" << h->events << " pending=" << state.needs_frame
        << " lifecycle=" << state.lifecycle << " surface=" << state.surface_generation
        << " error=" << h->error << " dark=" << h->workbench.dark()
        << " scroll=" << (scroll ? scroll->scrollOffset() : 0) << " page=" << h->workbench.page()
        << " renderer=" << h->surface.renderer() << " scale=" << h->scale
        << " detail=" << h->surface.error();
    int buttons = 0, sliders = 0;
    controls(out, h->workbench.page()->widget(), buttons, sliders);
    h->workbench.describeControls(out);
    out << h->workbench.diagnostics();
    __android_log_print(ANDROID_LOG_INFO, "OneUILab", "%s", out.str().c_str());
    return env->NewStringUTF(out.str().c_str());
}

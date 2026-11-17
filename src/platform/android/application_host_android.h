#pragma once
#include "oneui/mobile/application.h"
#include "platform/android/skia_surface_android.h"
#include <algorithm>
#include <jni.h>

namespace oneui::mobile::android {
struct ApplicationSurface final {
    ApplicationHost core{[](ApplicationServices &services) {
        return std::unique_ptr<Application>(oneui_create_mobile_application(&services));
    }};
    TextInputSession &input = core.input();
    SkiaSurface surface;
    float scale = 1;
    int width = 0, height = 0, editorInset = 0;
    bool reveal = false;
    int error = 0;
    uint64_t frames = 0; // Queued pictures; actual EGL swaps use RenderStatistics.
    Runtime &runtime() { return core.runtime(); }
    OneUiMobileResult layout() {
        return core.viewport({float(width), float(std::max(1, height - editorInset)), scale, 0, 0, 0, 0, 0});
    }
    void suspend() { core.surface(false); surface.reset(); }
};
ApplicationSurface *applicationSurface(JNIEnv *env, jlong handle);
}

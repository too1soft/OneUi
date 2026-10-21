#pragma once
#include "mobile/workbench.hpp"
#include "oneui/mobile/text_input.h"
#include "oneui/mobile/widget_host.h"
#include "platform/android/skia_surface_android.h"
#include <jni.h>
namespace lab::android {
using namespace oneui;
struct Host {
    oneui::mobile::WidgetHost widgets;
    oneui::mobile::SkiaSurface surface;
    lab::mobile::Workbench workbench;
    oneui::mobile::TextInputSession input{workbench.widget()};
    float scale = 1;
    unsigned frames = 0, events = 0, callbacks = 0;
    int error = 0;
    bool reveal = false;
    Host() {
        widgets.setTouchScrolling(true);
        widgets.setContent(workbench.widget());
    }
    oneui::mobile::Runtime &runtime() { return widgets.runtime(); }
    void activate() {
        OneUiMobileStateV1 state{};
        runtime().state(state);
        workbench.setActive(state.lifecycle == ONEUI_MOBILE_ACTIVE && state.surface_available);
    }
    void commands() {
        if (workbench.hasPending()) {
            runtime().cancelPointers();
            widgets.pumpInput();
            workbench.flush();
        }
        workbench.syncAppearance();
    }
    void suspend() {
        workbench.setActive(false);
        runtime().setSurface(false);
        widgets.pumpInput();
        surface.reset();
    }
};
Host *host(JNIEnv *, jlong);
} // namespace lab::android

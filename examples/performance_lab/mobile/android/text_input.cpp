#include "host.hpp"
#include <jni.h>

using namespace lab::android;
#define JNI_METHOD(name) Java_net_too1_oneui_lab_NativeHost_##name
extern "C" JNIEXPORT jobject JNICALL JNI_METHOD(editing)(JNIEnv *env, jclass, jlong value) {
    auto *h = host(env, value);
    if (!h)
        return nullptr;
    OneUiMobileStateV1 runtime{};
    h->runtime().state(runtime);
    if (runtime.lifecycle != ONEUI_MOBILE_ACTIVE)
        return nullptr;
    const auto s = h->input.snapshot();
    auto cls = env->FindClass("net/too1/oneui/EditorSurface$Snapshot");
    auto ctor = env->GetMethodID(cls, "<init>", "(JLjava/lang/String;IIIIZZ)V");
    auto text =
        env->NewString(reinterpret_cast<const jchar *>(s.text.data()), jsize(s.text.size()));
    return env->NewObject(cls, ctor, jlong(s.session), text, s.anchor, s.caret, s.composingStart,
                          s.composingEnd, jboolean(s.multiline), jboolean(s.sensitive));
}
extern "C" JNIEXPORT jboolean JNICALL JNI_METHOD(edit)(JNIEnv *env, jclass, jlong value,
                                                       jlong session, jstring text, jint anchor,
                                                       jint caret, jint start, jint end) {
    auto *h = host(env, value);
    if (!h || !text)
        return false;
    OneUiMobileStateV1 runtime{};
    h->runtime().state(runtime);
    if (runtime.lifecycle != ONEUI_MOBILE_ACTIVE)
        return false;
    const auto length = env->GetStringLength(text);
    if (length > 1024 * 1024)
        return false;
    const auto *chars = env->GetStringChars(text, nullptr);
    if (!chars)
        return false;
    oneui::mobile::EditingState s;
    s.session = uint64_t(session);
    s.text.assign(reinterpret_cast<const char16_t *>(chars), size_t(length));
    env->ReleaseStringChars(text, chars);
    s.anchor = anchor;
    s.caret = caret;
    s.composingStart = start;
    s.composingEnd = end;
    const bool accepted = h->input.apply(s);
    h->reveal = h->reveal || accepted;
    return accepted;
}
extern "C" JNIEXPORT void JNICALL JNI_METHOD(typography)(JNIEnv *env, jclass, jlong value,
                                                         jfloat scale, jfloat size) {
    if (auto *h = host(env, value))
        h->workbench.typography(scale, size);
}

extern "C" JNIEXPORT jboolean JNICALL JNI_METHOD(sensitiveEdit)(JNIEnv *env, jclass, jlong value,
                                                                jlong session, jint command,
                                                                jstring text, jint first,
                                                                jint second) {
    auto *h = host(env, value);
    if (!h || !text || command < 0 || command > 7)
        return false;
    OneUiMobileStateV1 runtime{};
    h->runtime().state(runtime);
    if (runtime.lifecycle != ONEUI_MOBILE_ACTIVE)
        return false;
    auto length = env->GetStringLength(text);
    if (length > 1024 * 1024)
        return false;
    auto chars = env->GetStringChars(text, nullptr);
    if (!chars)
        return false;
    std::u16string input(reinterpret_cast<const char16_t *>(chars), size_t(length));
    env->ReleaseStringChars(text, chars);
    bool accepted = h->input.editSensitive(uint64_t(session), oneui::mobile::SensitiveEdit(command),
                                           input, first, second);
    h->reveal = h->reveal || accepted;
    return accepted;
}
extern "C" JNIEXPORT jboolean JNICALL JNI_METHOD(containsEditor)(JNIEnv *env, jclass, jlong value,
                                                                 jfloat x, jfloat y) {
    auto *h = host(env, value);
    return h && h->input.containsPoint({x / h->scale, y / h->scale});
}
extern "C" JNIEXPORT jobject JNICALL JNI_METHOD(selectionGeometry)(JNIEnv *env, jclass,
                                                                   jlong value) {
    auto *h = host(env, value);
    if (!h)
        return nullptr;
    OneUiMobileStateV1 runtime{};
    h->runtime().state(runtime);
    if (runtime.lifecycle != ONEUI_MOBILE_ACTIVE || !runtime.surface_available)
        return nullptr;
    const auto s = h->input.selectionGeometry();
    if (!s.session)
        return nullptr;
    jfloat rects[12];
    int i = 0;
    for (const auto box : {s.clip, s.anchorRect, s.caretRect}) {
        rects[i++] = box.x * h->scale;
        rects[i++] = box.y * h->scale;
        rects[i++] = (box.x + box.width) * h->scale;
        rects[i++] = (box.y + box.height) * h->scale;
    }
    auto array = env->NewFloatArray(12);
    if (!array)
        return nullptr;
    env->SetFloatArrayRegion(array, 0, 12, rects);
    auto cls = env->FindClass("net/too1/oneui/EditorSurface$SelectionGeometry");
    auto ctor = env->GetMethodID(cls, "<init>", "(JII[F)V");
    return env->NewObject(cls, ctor, jlong(s.session), s.anchor, s.caret, array);
}
extern "C" JNIEXPORT jboolean JNICALL JNI_METHOD(selectAt)(JNIEnv *env, jclass, jlong value,
                                                           jlong session, jint edge, jfloat x,
                                                           jfloat y) {
    auto *h = host(env, value);
    if (!h || edge < 0 || edge > 2)
        return false;
    OneUiMobileStateV1 runtime{};
    h->runtime().state(runtime);
    return runtime.lifecycle == ONEUI_MOBILE_ACTIVE && runtime.surface_available &&
           h->input.selectAtPoint(uint64_t(session), oneui::mobile::SelectionEdge(edge),
                                  {x / h->scale, y / h->scale});
}

#include "oneui/vpn/c_api.h"
#include "oneui/vpn/json_api.h"
#include <jni.h>
#include <cstring>
#include <string>

namespace {
struct Binding {
    JNIEnv *env;
    jobject owner;
    OneUiVpnSession *session = nullptr;
    jmethodID begin, end;
    static void start(void *context, uint64_t attempt, const char *json) {
        auto &self = *static_cast<Binding *>(context);
        auto bytes = self.env->NewByteArray(static_cast<jsize>(std::strlen(json)));
        if (!bytes) return;
        self.env->SetByteArrayRegion(bytes, 0, self.env->GetArrayLength(bytes), reinterpret_cast<const jbyte *>(json));
        self.env->CallVoidMethod(self.owner, self.begin, static_cast<jlong>(attempt), bytes);
        self.env->DeleteLocalRef(bytes);
    }
    static void stop(void *context, uint64_t attempt) {
        auto &self = *static_cast<Binding *>(context);
        self.env->CallVoidMethod(self.owner, self.end, static_cast<jlong>(attempt));
    }
};
Binding *binding(jlong value) { return reinterpret_cast<Binding *>(value); }

}
extern "C" JNIEXPORT jlong JNICALL Java_net_too1_oneui_vpn_OneUiVpnSession_create(JNIEnv *env, jobject owner) {
    auto *self = new Binding{env, env->NewGlobalRef(owner)};
    auto cls = env->GetObjectClass(owner);
    self->begin = env->GetMethodID(cls, "begin", "(J[B)V");
    self->end = env->GetMethodID(cls, "end", "(J)V");
    env->DeleteLocalRef(cls);
    OneUiVpnHostV1 host{sizeof(host), self, Binding::start, Binding::stop};
    if (!self->owner || !self->begin || !self->end || oneui_vpn_create_v1(&host, &self->session) != ONEUI_VPN_OK) {
        if (self->owner) env->DeleteGlobalRef(self->owner);
        delete self; return 0;
    }
    return reinterpret_cast<jlong>(self);
}
extern "C" JNIEXPORT jint JNICALL Java_net_too1_oneui_vpn_OneUiVpnSession_connect(JNIEnv *env, jclass, jlong handle, jbyteArray bytes) {
    if (!handle || !bytes || env->GetArrayLength(bytes) > 65536) return ONEUI_VPN_INVALID_ARGUMENT;
    try {
        std::string text(env->GetArrayLength(bytes), '\0');
        env->GetByteArrayRegion(bytes, 0, static_cast<jsize>(text.size()), reinterpret_cast<jbyte *>(text.data()));
        return oneui_vpn_connect_json_v1(binding(handle)->session, text.data(), text.size());
    } catch (...) { return ONEUI_VPN_INVALID_ARGUMENT; }
}
extern "C" JNIEXPORT jint JNICALL Java_net_too1_oneui_vpn_OneUiVpnSession_disconnect(JNIEnv *, jclass, jlong handle) {
    return handle ? oneui_vpn_disconnect(binding(handle)->session) : ONEUI_VPN_INVALID_ARGUMENT;
}
extern "C" JNIEXPORT jint JNICALL Java_net_too1_oneui_vpn_OneUiVpnSession_report(JNIEnv *, jclass, jlong handle, jlong attempt, jint event, jint code) {
    return handle ? oneui_vpn_report_v1(binding(handle)->session, attempt, event, code) : ONEUI_VPN_INVALID_ARGUMENT;
}
extern "C" JNIEXPORT jlongArray JNICALL Java_net_too1_oneui_vpn_OneUiVpnSession_snapshot(JNIEnv *env, jclass, jlong handle) {
    OneUiVpnSnapshotV1 value{}; value.struct_size = sizeof(value);
    if (!handle || oneui_vpn_snapshot_v1(binding(handle)->session, &value) != ONEUI_VPN_OK) return nullptr;
    const jlong items[]{value.state, value.error_code, static_cast<jlong>(value.attempt), static_cast<jlong>(value.revision)};
    auto result = env->NewLongArray(4); if (result) env->SetLongArrayRegion(result, 0, 4, items);
    return result;
}
extern "C" JNIEXPORT jint JNICALL Java_net_too1_oneui_vpn_OneUiVpnSession_destroy(JNIEnv *env, jclass, jlong handle) {
    if (!handle) return ONEUI_VPN_INVALID_ARGUMENT;
    auto *self = binding(handle);
    const auto result = oneui_vpn_destroy(self->session);
    if (result == ONEUI_VPN_OK) { env->DeleteGlobalRef(self->owner); delete self; }
    return result;
}

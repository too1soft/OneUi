#include "oneui/mobile/frame_mailbox.h"
#include <jni.h>
extern "C" JNIEXPORT jboolean JNICALL Java_net_too1_oneui_webrtc_FrameReceiver_submit(
    JNIEnv *env, jclass, jlong handle, jobject buffer, jint width, jint height, jint stride) {
    if (!handle || !buffer)
        return false;
    void *data = env->GetDirectBufferAddress(buffer);
    const auto bytes = env->GetDirectBufferCapacity(buffer);
    return bytes > 0 && reinterpret_cast<oneui::mobile::FrameMailbox *>(handle)->submitNv12(
                            data, static_cast<size_t>(bytes), width, height, stride);
}

package net.too1.oneui.mobileprobe;

import android.view.Surface;

// Private test-host JNI bridge. The SDK's public C ABI remains version 1.
final class NativeHost {
    static { System.loadLibrary("oneui_mobile_probe"); }
    static native long create();
    static native void destroy(long handle);
    static native void lifecycle(long handle, int state);
    static native boolean attach(long handle, Surface surface, int width, int height, float scale);
    static native void detach(long handle);
    static native void pointer(long handle, int id, int phase, int kind,
                               float x, float y, float pressure, long time);
    static native void cancel(long handle);
    static native boolean needsFrame(long handle);
    static native void draw(long handle, long time);
    static native String status(long handle);
}

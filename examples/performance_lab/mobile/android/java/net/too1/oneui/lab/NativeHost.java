package net.too1.oneui.lab;

import android.view.Surface;
import net.too1.oneui.EditorSurface;

// Private test-host JNI bridge. The SDK's public C ABI remains version 1.
final class NativeHost {
    static { System.loadLibrary("oneui_mobile_lab"); }
    static native EditorSurface.Snapshot editing(long host);
    static native boolean edit(long host, long session, String text, int anchor, int caret, int start, int end);
    static native boolean sensitiveEdit(long host, long session, int command, String text, int first, int second);
    static native boolean containsEditor(long host, float x, float y);
    static native EditorSurface.SelectionGeometry selectionGeometry(long host);
    static native boolean selectAt(long host, long session, int edge, float x, float y);
    static native void typography(long host, float scale, float size);
    static native long create();
    static native void destroy(long handle);
    static native void lifecycle(long handle, int state);
    static native boolean attach(long handle, Surface surface, int width, int height, float scale);
    static native void detach(long handle);
    static native void pointer(long handle, int id, int phase, int kind,
                               float x, float y, float pressure, long time);
    static native void cancel(long handle);
    static native boolean back(long handle);
    static native boolean needsFrame(long handle);
    static native void draw(long handle, long time);
    static native String status(long handle);
    static native void observeAppearance(long handle, int theme, boolean available);
    static native long[] appearance(long handle);
    static native void acknowledgeAppearance(long handle, long revision);
}

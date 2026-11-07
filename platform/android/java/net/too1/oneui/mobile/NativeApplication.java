package net.too1.oneui.mobile;

import android.view.Surface;
import net.too1.oneui.EditorSurface;

/** Linked from OneUI's prebuilt host archive into the application widget module.
 * Its Activity loads that module before invoking these owner-thread methods. */
public final class NativeApplication {
    private NativeApplication() { }
    public static native long create();
    public static native void destroy(long host);
    public static native void lifecycle(long host, int state);
    public static native boolean attach(long host, Surface surface, int width, int height, float scale);
    public static native void editorInset(long host, int pixels);
    public static native void detach(long host);
    public static native void pointer(long host, int id, int phase, int kind, float x, float y, float pressure, long time);
    public static native void cancel(long host);
    public static native boolean back(long host);
    public static native boolean needsFrame(long host);
    public static native int wakeDelayMillis(long host);
    public static native boolean draw(long host, long time);
    public static native void observeAppearance(long host, int theme, boolean available);
    public static native long[] appearance(long host);
    public static native void acknowledgeAppearance(long host, long revision);
    public static native NativeRequest[] requests(long host);
    public static native void reply(long host, long request, int code, byte[] payload);
    public static native EditorSurface.Snapshot editing(long host);
    public static native boolean edit(long host, long session, String text, int anchor, int caret, int start, int end);
    public static native boolean sensitiveEdit(long host, long session, int command, String text, int first, int second);
    public static native boolean containsEditor(long host, float x, float y);
    public static native EditorSurface.SelectionGeometry selectionGeometry(long host);
    public static native boolean selectAt(long host, long session, int edge, float x, float y);
    public static native NativeUiNode[] inspect(long host);
    public static native void typography(long host, float scale, float bodySize);
}

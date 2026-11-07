package net.too1.oneui.mobileprobe;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Intent;
import android.os.SystemClock;
import android.view.InputDevice;
import android.view.MotionEvent;
import java.lang.reflect.Field;
import java.util.concurrent.Callable;
import java.util.concurrent.atomic.AtomicReference;
import net.too1.oneui.EditorSurface;
import net.too1.oneui.mobile.NativeApplication;
import net.too1.oneui.mobile.NativeUiNode;

/** Generic real-Android presentation driver. Business fixtures live in consumers. */
public abstract class NativePresentationProbe extends Instrumentation {
    protected Activity activity;
    protected EditorSurface surface;
    protected long host;
    protected float density;

    protected <T> T ui(Callable<T> action) {
        AtomicReference<T> value = new AtomicReference<>();
        AtomicReference<Throwable> failure = new AtomicReference<>();
        runOnMainSync(() -> { try { value.set(action.call()); } catch (Throwable e) { failure.set(e); } });
        if (failure.get() != null) throw new AssertionError("Native UI operation failed", failure.get());
        return value.get();
    }

    protected void open(String className, String scenario) throws Exception {
        if (activity != null) ui(() -> { activity.finish(); return null; });
        activity = startActivitySync(new Intent().setClassName(getTargetContext(), className)
            .putExtra("scenario", scenario).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK));
        waitForIdleSync();
        ui(() -> {
            Class<?> type = Class.forName("net.too1.oneui.mobile.OneUiActivity");
            Field h = type.getDeclaredField("host"), s = type.getDeclaredField("surface");
            h.setAccessible(true); s.setAccessible(true);
            host = h.getLong(activity); surface = (EditorSurface)s.get(activity);
            density = activity.getResources().getDisplayMetrics().density;
            activity.getWindow().addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            activity.setTurnScreenOn(true);
            return null;
        });
        SystemClock.sleep(250);
        waitForIdleSync();
        hideKeyboard();
    }

    protected NativeUiNode[] nodes() { return ui(() -> NativeApplication.inspect(host)); }
    protected NativeUiNode node(String name) {
        NativeUiNode found = null;
        for (NativeUiNode item : nodes()) if (name.equals(item.name)) found = item;
        return found;
    }
    protected NativeUiNode await(String name) {
        long end = SystemClock.uptimeMillis() + 6000;
        do {
            NativeUiNode item = node(name);
            if (item != null && !item.disabled && item.height > 0) return item;
            SystemClock.sleep(30);
        } while (SystemClock.uptimeMillis() < end);
        NativeUiNode last = node(name);
        throw new AssertionError("Native control unavailable: " + name +
            (last == null ? "; absent" : "; disabled=" + last.disabled + "; height=" + last.height));
    }

    protected void pointer(float x, float y, int action, long down, long now) {
        MotionEvent event = MotionEvent.obtain(down, now, action, x, y, 0);
        event.setSource(InputDevice.SOURCE_TOUCHSCREEN);
        try { sendPointerSync(event); } finally { event.recycle(); }
    }
    protected void scroll(boolean down) {
        int[] origin = ui(() -> { int[] p = new int[2]; surface.getLocationOnScreen(p); return p; });
        float x = origin[0] + surface.getWidth() * .8f;
        float start = origin[1] + surface.getHeight() * (down ? .74f : .3f);
        float end = origin[1] + surface.getHeight() * (down ? .3f : .74f);
        long time = SystemClock.uptimeMillis();
        pointer(x, start, MotionEvent.ACTION_DOWN, time, time);
        for (int i = 1; i <= 10; i++) {
            SystemClock.sleep(16);
            pointer(x, start + (end - start) * i / 10, MotionEvent.ACTION_MOVE, time, SystemClock.uptimeMillis());
        }
        pointer(x, end, MotionEvent.ACTION_UP, time, SystemClock.uptimeMillis());
        SystemClock.sleep(300);
        waitForIdleSync();
    }
    protected NativeUiNode seek(String name) {
        NativeUiNode item = node(name);
        if (item != null && !item.disabled && item.height > 0) return item;
        // The semantic tree contains visible controls only. Locate offscreen
        // controls through real gestures, without bypassing viewport clipping.
        for (int i = 0; i < 4; i++) {
            scroll(false); item = node(name);
            if (item != null && !item.disabled && item.height > 0) return item;
        }
        for (int i = 0; i < 16; i++) {
            scroll(true); item = node(name);
            if (item != null && !item.disabled && item.height > 0) return item;
        }
        return await(name);
    }
    protected void click(String name) {
        NativeUiNode item = seek(name);
        for (int i = 0; i < 12; i++) {
            float bottom = surface.getHeight() / density - 4;
            // AccessibilityRole::Tab=10. Reserve actual navigation geometry;
            // auth pages have no bottom tabs, especially with the IME visible.
            if (item.role != 10)
                for (NativeUiNode tab : nodes()) if (tab.role == 10) bottom = Math.min(bottom, tab.y - 4);
            if (item.layoutY >= editorViewportTop() && item.layoutY + item.layoutHeight <= bottom) break;
            scroll(item.layoutY + item.layoutHeight > bottom); item = seek(name);
        }
        if (item.layoutHeight < 48 || item.layoutWidth < 48) throw new AssertionError("Touch target below48dp: " + name);
        if (item.y < 0 || item.y + item.height > surface.getHeight() / density)
            throw new AssertionError("Control unreachable by scrolling: " + name);
        int[] origin = ui(() -> { int[] p = new int[2]; surface.getLocationOnScreen(p); return p; });
        float x = origin[0] + (item.x + item.width / 2) * density;
        float y = origin[1] + (item.y + item.height / 2) * density;
        long time = SystemClock.uptimeMillis();
        pointer(x, y, MotionEvent.ACTION_DOWN, time, time);
        pointer(x, y, MotionEvent.ACTION_UP, time, time + 20);
        waitForIdleSync();
        SystemClock.sleep(100);
    }
    protected void type(String name, String text) {
        click(name);
        ui(() -> {
            android.view.inputmethod.InputConnection connection = surface.onCreateInputConnection(
                new android.view.inputmethod.EditorInfo());
            if (connection == null || !connection.commitText(text, 1))
                throw new AssertionError("Actual Android IME edit rejected");
            surface.sync(false);
            return null;
        });
        SystemClock.sleep(300);
        waitForIdleSync();
    }
    protected void hideKeyboard() {
        ui(() -> {
            activity.getSystemService(android.view.inputmethod.InputMethodManager.class)
                .hideSoftInputFromWindow(surface.getWindowToken(), 0);
            return null;
        });
        long end = SystemClock.uptimeMillis() + 2500;
        while (SystemClock.uptimeMillis() < end && ui(() -> surface.getRootWindowInsets() != null &&
                surface.getRootWindowInsets().isVisible(android.view.WindowInsets.Type.ime())))
            SystemClock.sleep(50);
        SystemClock.sleep(200);
    }
    /** A system/compatibility window can take focus without pausing the Activity. */
    protected void assertTransientWindowFocus(String name, boolean sensitive) {
        assertEditorVisible(name, sensitive);
        long session = ui(() -> NativeApplication.editing(host).session);
        android.app.Dialog dialog = ui(() -> {
            android.app.Dialog value = new android.app.Dialog(activity);
            value.setContentView(new android.widget.TextView(activity));
            value.show();
            return value;
        });
        try {
            SystemClock.sleep(250);
            if (ui(() -> activity.hasWindowFocus()))
                throw new AssertionError("Focus-loss fixture did not take window focus");
            EditorSurface.Snapshot current = ui(() -> NativeApplication.editing(host));
            if (current == null || current.session != session)
                throw new AssertionError("Temporary window focus loss reset the editor session");
        } finally { ui(() -> { dialog.dismiss(); return null; }); }
        SystemClock.sleep(300);
        assertEditorVisible(name, sensitive);
    }
    protected void assertOutsideTapKeepsKeyboardHidden() {
        hideKeyboard();
        int[] origin = ui(() -> { int[] p = new int[2]; surface.getLocationOnScreen(p); return p; });
        long time = SystemClock.uptimeMillis();
        float x = origin[0] + surface.getWidth() - 16 * density, y = origin[1] + 24 * density;
        pointer(x, y, MotionEvent.ACTION_DOWN, time, time);
        pointer(x, y, MotionEvent.ACTION_UP, time, time + 20);
        SystemClock.sleep(350);
        if (ui(() -> surface.getRootWindowInsets().isVisible(android.view.WindowInsets.Type.ime())))
            throw new AssertionError("Non-editor tap reopened the keyboard");
    }
    /** Consumer supplies the actual fixed chrome boundary, in native DIPs. */
    protected float editorViewportTop() { return 0; }
    protected void assertEditorVisible(String name, boolean sensitive) {
        long end = SystemClock.uptimeMillis() + 2500;
        while (SystemClock.uptimeMillis() < end && !ui(() -> surface.getRootWindowInsets() != null &&
                surface.getRootWindowInsets().isVisible(android.view.WindowInsets.Type.ime())))
            SystemClock.sleep(50);
        NativeUiNode field = node(name);
        EditorSurface.Snapshot editing = ui(() -> NativeApplication.editing(host));
        boolean ime = ui(() -> surface.getRootWindowInsets() != null &&
            surface.getRootWindowInsets().isVisible(android.view.WindowInsets.Type.ime()));
        float top = editorViewportTop(), bottom = surface.getHeight() / density;
        boolean oversized = field != null && field.layoutHeight > bottom - top;
        float visible = field == null ? 0 : Math.min(bottom, field.layoutY + field.layoutHeight) - Math.max(top, field.layoutY);
        boolean fits = field != null && (oversized
            ? visible >= Math.min(bottom - top, 28 * activity.getResources().getConfiguration().fontScale) - .5f
            : field.layoutY >= top - .5f && field.layoutY + field.layoutHeight <= bottom + .5f);
        if (!ime || editing == null || editing.session == 0 || editing.sensitive != sensitive ||
                !fits)
            throw new AssertionError("Focused editor visibility: " + name + ", ime=" + ime +
                ", editing=" + (editing != null && editing.session != 0) +
                ", sensitive=" + (editing != null && editing.sensitive) +
                ", field_y=" + (field == null ? -1 : field.y) +
                ", field_height=" + (field == null ? -1 : field.height) +
                ", viewport=" + surface.getHeight() / density);
        // Selection handles are deliberately unavailable for password fields.
        if (!sensitive) {
            EditorSurface.SelectionGeometry geometry = ui(() -> NativeApplication.selectionGeometry(host));
            if (geometry == null || !geometry.clip.contains(geometry.caretRect))
                throw new AssertionError("Editor caret outside its visible clipping region");
        }
    }
}

package net.too1.oneui.inputtests;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Intent;
import android.graphics.PointF;
import android.graphics.RectF;
import android.os.Bundle;
import android.os.SystemClock;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import net.too1.oneui.EditorSurface;
import static net.too1.oneui.inputtests.InputProtocol.check;

/** Sends real touchscreen events through Android's input dispatcher. Only the
 * starting text/selection use the public InputConnection, as an IME would. */
public final class SelectionProtocol extends Instrumentation {
    private EditorSurface surface;
    private volatile boolean ready;
    private boolean multiline;
    private long down;
    private android.app.UiAutomation automation;
    private final StringBuilder checks = new StringBuilder();
    private final android.content.BroadcastReceiver receiver = new android.content.BroadcastReceiver() {
        @Override public void onReceive(android.content.Context context, Intent intent) { ready = true; }
    };
    @Override public void onCreate(Bundle args) {
        super.onCreate(args); multiline = args != null && "multiline".equals(args.getString("mode")); start();
    }
    private void ui(Runnable action) {
        Throwable[] error = {null};
        runOnMainSync(() -> { try { action.run(); } catch(Throwable e) { error[0] = e; } });
        if (error[0] != null) throw new AssertionError(error[0]);
    }
    private InputConnection connection() { return surface.onCreateInputConnection(new EditorInfo()); }
    private int[] selection() {
        int[] result = new int[2];
        ui(() -> { EditorInfo info = new EditorInfo(); surface.onCreateInputConnection(info);
            result[0] = info.initialSelStart; result[1] = info.initialSelEnd; });
        return result;
    }
    private PointF point(int offset) {
        PointF result = new PointF();
        ui(() -> {
            EditorInfo info = new EditorInfo(); InputConnection c = surface.onCreateInputConnection(info);
            check(c.setSelection(offset, offset), "prepare target position");
            RectF r = surface.getSelectionGeometry().caretRect;
            result.set(r.left, r.centerY());
            check(c.setSelection(info.initialSelStart, info.initialSelEnd), "restore target probe");
        });
        return result;
    }
    private void event(int action, PointF a, PointF b) {
        int count = b == null ? 1 : 2;
        MotionEvent.PointerProperties[] properties = new MotionEvent.PointerProperties[count];
        MotionEvent.PointerCoords[] coords = new MotionEvent.PointerCoords[count];
        int[] location = new int[2]; ui(() -> surface.getLocationOnScreen(location));
        for (int i = 0; i < count; ++i) {
            properties[i] = new MotionEvent.PointerProperties(); properties[i].id = i;
            properties[i].toolType = MotionEvent.TOOL_TYPE_FINGER;
            PointF p = i == 0 ? a : b;
            coords[i] = new MotionEvent.PointerCoords(); coords[i].x = location[0]+p.x;
            coords[i].y = location[1]+p.y; coords[i].pressure = 1; coords[i].size = 1;
        }
        if (action == MotionEvent.ACTION_DOWN) down = SystemClock.uptimeMillis();
        MotionEvent e = MotionEvent.obtain(down, SystemClock.uptimeMillis(), action, count, properties,
            coords, 0, 0, 1, 1, 0, 0, InputDevice.SOURCE_TOUCHSCREEN, 0);
        // Explicit pointer streams are needed for CANCEL and multiple fingers.
        check(automation.injectInputEvent(e, true), "pointer injection failed");
        e.recycle(); SystemClock.sleep(35);
    }
    private void tap(PointF p, boolean hold) {
        int[] location = new int[2]; ui(() -> surface.getLocationOnScreen(location));
        String xy = Math.round(location[0]+p.x)+" "+Math.round(location[1]+p.y);
        // UiAutomation.injectInputEvent waits for Surface operations between
        // DOWN and UP (observed >3s on this emulator). Shell input delivers the
        // complete timed gesture through InputManager without that barrier.
        String command = hold ? "input touchscreen swipe "+xy+" "+xy+" 1500" : "input touchscreen tap "+xy;
        try (android.os.ParcelFileDescriptor fd = automation.executeShellCommand(command);
             java.io.FileInputStream stream = new java.io.FileInputStream(fd.getFileDescriptor())) {
            byte[] buffer = new byte[256]; while (stream.read(buffer) >= 0) { }
        } catch (java.io.IOException error) { throw new AssertionError(error); }
        SystemClock.sleep(250);
    }
    private void record(String name) { checks.append("PASS: ").append(name).append('\n'); }
    private PointF[] handle(int index, int targetOffset) {
        PointF destination = point(targetOffset);
        PointF[] points = {new PointF(), new PointF()};
        ui(() -> {
            float[] b = surface.getSelectionHandleBounds(); int i = index * 4;
            check(b[i+2] > b[i] && b[i+3] > b[i+1], "handle not visible");
            EditorSurface.SelectionGeometry g = surface.getSelectionGeometry();
            RectF r = index == 0 ? g.anchorRect : g.caretRect;
            points[0].set((b[i]+b[i+2])/2, (b[i+1]+b[i+3])/2);
            points[1].set(points[0].x + destination.x-r.left, points[0].y + destination.y-r.centerY());
        });
        return points;
    }
    private void drag(int handle, int offset, int finish) {
        PointF[] p = handle(handle, offset);
        event(MotionEvent.ACTION_DOWN, p[0], null);
        ui(() -> check(surface.isSelectionGestureActive(), "touch did not capture handle"));
        event(MotionEvent.ACTION_MOVE, p[1], null);
        if (finish == 2) {
            PointF second = new PointF(p[1].x+25, p[1].y);
            event(MotionEvent.ACTION_POINTER_DOWN | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT), p[1], second);
            event(MotionEvent.ACTION_POINTER_UP | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT), p[1], second);
            event(MotionEvent.ACTION_UP, p[1], null);
        } else event(finish == 1 ? MotionEvent.ACTION_CANCEL : MotionEvent.ACTION_UP, p[1], null);
        SystemClock.sleep(150);
        ui(() -> check(!surface.isSelectionGestureActive(), "gesture capture leaked"));
    }
    private void expect(int a, int b, String why) {
        int[] s = selection(); check(s[0] == a && s[1] == b, why+": "+s[0]+","+s[1]);
    }
    private void verify() {
        SystemClock.sleep(1200); // Let the initial focus/IME resize finish.
        ui(() -> { InputConnection c = connection(); check(c != null, "no editor");
            check(c.commitText(multiline ? "alpha beta\nsecond line" : "alpha beta gamma", 1), "seed text"); });
        tap(point(2), false);
        expect(2, 2, "tap caret"); record("tap-positions-caret");
        // Let Android finish showing the IME before acquiring its new geometry.
        SystemClock.sleep(1200);
        drag(0, 4, 0); expect(4, 4, "insertion drag"); record("insertion-handle-drag");
        tap(point(7), true); expect(6, 10, "long press word"); record("native-long-press-word");
        drag(0, 2, 0); expect(2, 10, "anchor drag");
        drag(1, 8, 0); expect(2, 8, "caret endpoint drag"); record("both-selection-endpoints");
        drag(0, 13, 0); expect(13, 8, "crossed selection"); record("crossed-selection-retains-endpoint");
        drag(0, 3, 1); expect(13, 8, "cancel rollback"); record("cancel-rolls-back-selection");
        drag(0, 3, 2); expect(13, 8, "multi-touch rollback"); record("multitouch-cancels-selection");
        PointF[] resizeDrag = handle(0, 3);
        event(MotionEvent.ACTION_DOWN, resizeDrag[0], null);
        event(MotionEvent.ACTION_MOVE, resizeDrag[1], null);
        ui(() -> { surface.cancelSelectionGesture();
            check(!surface.isSelectionGestureActive(), "resize left an active gesture");
            float[] b = surface.getSelectionHandleBounds();
            check(b[2] == b[0] && b[6] == b[4], "resize left active handles"); });
        event(MotionEvent.ACTION_UP, resizeDrag[1], null);
        expect(13, 8, "resize rollback"); record("surface-change-cancels-drag");
        if (multiline) record("multiline-handle-geometry");
        ui(() -> {
            InputConnection c = connection(); c.setSelection(0, multiline ? 22 : 16);
            check(c.commitText("a😀e\u0301 z", 1), "unicode seed");
        });
        tap(point(3), false); expect(3,3,"UTF16 caret");
        drag(0, 5, 0); expect(5,5,"combining grapheme caret"); record("unicode-surrogate-grapheme-position");
        ui(() -> { surface.suspend(); float[] b = surface.getSelectionHandleBounds();
            check(b[2] == b[0] && b[6] == b[4], "suspend leaked handles"); surface.resume(); });
        record("suspend-removes-handles");
    }
    @Override public void onStart() {
        Bundle result = new Bundle(); boolean registered = false;
        try {
            getTargetContext().registerReceiver(receiver, new android.content.IntentFilter("net.too1.oneui.inputtests.RUN"),
                "android.permission.DUMP", null, android.content.Context.RECEIVER_EXPORTED); registered = true;
            Intent launch = getTargetContext().getPackageManager().getLaunchIntentForPackage("net.too1.oneui.lab");
            launch.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK); Activity activity = startActivitySync(launch);
            for (int i=0;i<300 && !ready;++i) SystemClock.sleep(200);
            check(ready, "external driver timeout");
            ui(() -> { surface = InputProtocol.find(activity.getWindow().getDecorView());
                check(surface != null && surface.onCheckIsTextEditor(), "driver did not focus editor"); });
            // Connecting UiAutomation can take longer than a long-press timeout;
            // complete it before assigning any gesture's downTime.
            automation = getUiAutomation();
            verify(); result.putString("stream", checks.toString());
            finish(Activity.RESULT_OK, result);
        } catch(Throwable e) {
            result.putString("stream", checks+"FAIL: "+e.toString()+"\n"); finish(Activity.RESULT_CANCELED,result);
        } finally { if (registered) getTargetContext().unregisterReceiver(receiver); }
    }
}

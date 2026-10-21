package net.too1.oneui.lab;

import android.app.Activity;
import android.content.Intent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.IntentFilter;
import android.content.pm.ApplicationInfo;
import android.content.res.Configuration;
import android.graphics.Color;
import android.os.Build;
import android.os.Bundle;
import android.view.Choreographer;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;
import android.window.OnBackInvokedCallback;
import android.window.OnBackInvokedDispatcher;
import net.too1.oneui.SystemAppearance;
import net.too1.oneui.EditorSurface;
import android.util.TypedValue;

// Android owns lifecycle and Surface only. Page, controls, fonts and theme are
// the same C++ OneUI widgets used by the desktop workbench.
public final class LabActivity extends Activity implements SurfaceHolder.Callback,
        Choreographer.FrameCallback {
    private long host;
    private EditorSurface surface;
    private TextView error;
    private SystemAppearance chrome;
    private boolean started, resumed, pending, available;
    private boolean receiverRegistered;
    private Object backCallback;
    // Keep API 33 classes out of Activity initialization on older Android.
    private static final class BackApi33 {
        static Object register(Activity activity, Runnable action) {
            OnBackInvokedCallback callback = action::run;
            activity.getOnBackInvokedDispatcher().registerOnBackInvokedCallback(
                OnBackInvokedDispatcher.PRIORITY_DEFAULT, callback);
            return callback;
        }
        static void unregister(Activity activity, Object callback) {
            activity.getOnBackInvokedDispatcher().unregisterOnBackInvokedCallback((OnBackInvokedCallback) callback);
        }
    }
    private final BroadcastReceiver diagnostics = new BroadcastReceiver() {
        @Override public void onReceive(Context context, Intent intent) {
            if (host == 0) return;
            String command = intent.getStringExtra("command");
            if ("status".equals(command)) {
                float[] b = surface.getSelectionHandleBounds();
                setResultData(NativeHost.status(host) + chrome.diagnostics() +
                    " handle0="+b[0]+","+b[1]+","+b[2]+","+b[3] +
                    " handle1="+b[4]+","+b[5]+","+b[6]+","+b[7] +
                    " handleDrag="+(surface.isSelectionGestureActive()?1:0));
            }
            else if ("rebuild".equals(command)) {
                surface.removeCallbacks(restoreSurface);
                surface.setVisibility(View.GONE);
                surface.postDelayed(restoreSurface, 150);
            }
        }
    };
    private final Runnable restoreSurface = () -> {
        if (host != 0) surface.setVisibility(View.VISIBLE);
    };

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        host = NativeHost.create();
        chrome = new SystemAppearance(this);
        FrameLayout root = chrome.content();
        surface = new EditorSurface(this, new EditorSurface.Bridge() {
            public EditorSurface.Snapshot snapshot() { return host == 0 ? null : NativeHost.editing(host); }
            public boolean apply(long session, String text, int a, int b, int start, int end) {
                return host != 0 && NativeHost.edit(host, session, text, a, b, start, end);
            }
            public boolean sensitive(long session, int command, String text, int first, int second) {
                return host != 0 && NativeHost.sensitiveEdit(host, session, command, text, first, second);
            }
            public boolean containsEditor(float x, float y) { return host != 0 && NativeHost.containsEditor(host, x, y); }
            public EditorSurface.SelectionGeometry selectionGeometry() {
                return host == 0 ? null : NativeHost.selectionGeometry(host);
            }
            public boolean selectAt(long session, int edge, float x, float y) {
                return host != 0 && NativeHost.selectAt(host, session, edge, x, y);
            }
            public void cancelPointers() { if (host != 0) NativeHost.cancel(host); }
            public void changed() { requestFrame(); }
        });
        surface.setContentDescription("OneUI 试验台；布局、性能、功能目录及共享控件");
        root.addView(surface, new FrameLayout.LayoutParams(-1, -1));
        surface.getHolder().addCallback(this);
        surface.setOnTouchListener((view, event) -> touch(event));
        error = new TextView(this);
        error.setTextSize(16); error.setTextColor(Color.rgb(160, 43, 36));
        error.setBackgroundColor(Color.WHITE); error.setPadding(24, 24, 24, 24);
        error.setVisibility(View.GONE);
        root.addView(error, new FrameLayout.LayoutParams(-1, -2));
        setContentView(chrome);
        observeSystem();
        if (Build.VERSION.SDK_INT >= 33)
            backCallback = BackApi33.register(this, this::handleBack);
        if ((getApplicationInfo().flags & ApplicationInfo.FLAG_DEBUGGABLE) != 0) {
            IntentFilter filter = new IntentFilter("net.too1.oneui.lab.DIAGNOSTIC");
            // Shell diagnostic reads must not pause/resume the Activity or
            // schedule frames. DUMP restricts this exported receiver to shell /
            // privileged callers; ordinary installed apps cannot trigger it.
            if (Build.VERSION.SDK_INT >= 33)
                registerReceiver(diagnostics, filter, "android.permission.DUMP", null, Context.RECEIVER_EXPORTED);
            else registerReceiver(diagnostics, filter, "android.permission.DUMP", null);
            receiverRegistered = true;
        }
    }
    private void observeSystem() {
        NativeHost.observeAppearance(host, chrome.systemTheme(), true);
        android.util.DisplayMetrics metrics = getResources().getDisplayMetrics();
        float body = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_SP, 16, metrics) / metrics.density;
        NativeHost.typography(host, getResources().getConfiguration().fontScale, body);
    }
    private void syncAppearance() {
        if (host == 0 || chrome == null) return;
        long[] state = NativeHost.appearance(host);
        surface.setSelectionHandleColor(state[6] == 2 ? 0xffc4ed87 : 0xff28613d);
        if (state[0] != state[1] && chrome.apply((int) state[2], (int) state[3],
                state[4] == 1, state[5] == 1, state[6] == 2 ? 0xff171e21 : 0xfff5f7f4))
            NativeHost.acknowledgeAppearance(host, state[0]);
    }
    @Override public void onConfigurationChanged(Configuration config) {
        super.onConfigurationChanged(config);
        if (host != 0) observeSystem();
        syncAppearance(); requestFrame();
    }
    private void requestFrame() {
        if (host != 0 && available && resumed && !pending && NativeHost.needsFrame(host)) {
            pending = true; Choreographer.getInstance().postFrameCallback(this);
        }
    }
    private void stopFrame() { Choreographer.getInstance().removeFrameCallback(this); pending = false; }
    @Override public void doFrame(long time) {
        pending = false;
        if (host == 0 || !available || !resumed) return;
        NativeHost.draw(host, time); surface.sync(false); requestFrame();
    }
    @Override protected void onStart() { super.onStart(); started = true; }
    @Override protected void onResume() {
        super.onResume(); resumed = true;
        if (host != 0) observeSystem();
        syncAppearance(); activate();
    }
    private void activate() {
        if (host != 0) {
            NativeHost.lifecycle(host, !started ? 3 : resumed && hasWindowFocus() ? 1 : 2);
            if (resumed && hasWindowFocus()) surface.resume(); else surface.suspend();
            requestFrame();
        }
    }
    @Override public void onWindowFocusChanged(boolean focused) {
        super.onWindowFocusChanged(focused); if (!focused) stopFrame();
        syncAppearance(); activate();
    }
    @Override protected void onPause() {
        resumed = false; surface.suspend(); stopFrame(); if (host != 0) NativeHost.lifecycle(host, 2);
        super.onPause();
    }
    @Override protected void onStop() {
        started = false; if (host != 0) NativeHost.lifecycle(host, 3); super.onStop();
    }
    @Override protected void onDestroy() {
        stopFrame(); surface.suspend(); surface.removeCallbacks(restoreSurface);
        if (Build.VERSION.SDK_INT >= 33)
            BackApi33.unregister(this, backCallback);
        if (receiverRegistered) unregisterReceiver(diagnostics);
        if (host != 0) { NativeHost.destroy(host); host = 0; }
        super.onDestroy();
    }
    private void handleBack() {
        if (surface != null && surface.dismissEditingActions()) return;
        if (host != 0 && NativeHost.back(host)) { syncAppearance(); requestFrame(); }
        else finish();
    }
    @Override public void onBackPressed() { handleBack(); }
    @Override public void surfaceCreated(SurfaceHolder holder) { }
    @Override public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        surface.cancelSelectionGesture();
        stopFrame();
        available = host != 0 && NativeHost.attach(host, holder.getSurface(), width, height,
            getResources().getDisplayMetrics().density);
        error.setVisibility(available ? View.GONE : View.VISIBLE);
        if (!available) error.setText("OneUI 绘制表面创建失败。请返回后重新打开。\n" + NativeHost.status(host));
        syncAppearance();
        requestFrame();
    }
    @Override public void surfaceDestroyed(SurfaceHolder holder) {
        available = false; stopFrame(); if (host != 0) NativeHost.detach(host);
    }
    private boolean touch(MotionEvent event) {
        if (host == 0 || !available || !resumed || !hasWindowFocus()) return false;
        if (surface.beforeEditorTouch(event)) { requestFrame(); return true; }
        int action = event.getActionMasked();
        if (event.getPointerCount() > 16 || action == MotionEvent.ACTION_CANCEL) NativeHost.cancel(host);
        else if (action == MotionEvent.ACTION_MOVE) {
            for (int i = 0; i < event.getPointerCount(); ++i) pointer(event, i, 1);
        } else if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) pointer(event, event.getActionIndex(), 0);
        else if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_POINTER_UP) pointer(event, event.getActionIndex(), 2);
        else return false;
        surface.sync(false);
        boolean editingMenu = surface.onEditorTouch(event);
        if (action == MotionEvent.ACTION_UP && !editingMenu) surface.sync(true);
        syncAppearance(); requestFrame(); return true;
    }
    private void pointer(MotionEvent event, int index, int phase) {
        int tool = event.getToolType(index);
        if (tool != MotionEvent.TOOL_TYPE_FINGER && tool != MotionEvent.TOOL_TYPE_STYLUS && tool != MotionEvent.TOOL_TYPE_ERASER) return;
        NativeHost.pointer(host, event.getPointerId(index), phase, tool == MotionEvent.TOOL_TYPE_FINGER ? 0 : 1,
            event.getX(index), event.getY(index), Math.max(0, Math.min(1, event.getPressure(index))), event.getEventTime() * 1000000L);
    }
}

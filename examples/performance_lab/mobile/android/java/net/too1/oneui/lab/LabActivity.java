package net.too1.oneui.lab;

import android.app.Activity;
import android.content.Intent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.IntentFilter;
import android.content.pm.ApplicationInfo;
import android.graphics.Color;
import android.os.Build;
import android.os.Bundle;
import android.view.Choreographer;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowInsetsController;
import android.widget.FrameLayout;
import android.widget.TextView;
import android.window.OnBackInvokedCallback;
import android.window.OnBackInvokedDispatcher;

// Android owns lifecycle and Surface only. Page, controls, fonts and theme are
// the same C++ OneUI widgets used by the desktop workbench.
public final class LabActivity extends Activity implements SurfaceHolder.Callback,
        Choreographer.FrameCallback {
    private long host;
    private SurfaceView surface;
    private TextView error;
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
            if ("status".equals(command)) setResultData(NativeHost.status(host));
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
        FrameLayout root = new FrameLayout(this);
        root.setBackgroundColor(Color.rgb(245, 247, 244));
        root.setOnApplyWindowInsetsListener((view, insets) -> {
            root.setPadding(insets.getSystemWindowInsetLeft(), insets.getSystemWindowInsetTop(),
                insets.getSystemWindowInsetRight(), insets.getSystemWindowInsetBottom());
            return insets;
        });
        surface = new SurfaceView(this);
        surface.setContentDescription("OneUI 试验台；布局、性能、功能目录及共享控件");
        root.addView(surface, new FrameLayout.LayoutParams(-1, -1));
        surface.getHolder().addCallback(this);
        surface.setOnTouchListener((view, event) -> touch(event));
        error = new TextView(this);
        error.setTextSize(16); error.setTextColor(Color.rgb(160, 43, 36));
        error.setBackgroundColor(Color.WHITE); error.setPadding(24, 24, 24, 24);
        error.setVisibility(View.GONE);
        root.addView(error, new FrameLayout.LayoutParams(-1, -2));
        setContentView(root);
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
        if (Build.VERSION.SDK_INT >= 30) {
            int light = WindowInsetsController.APPEARANCE_LIGHT_STATUS_BARS |
                WindowInsetsController.APPEARANCE_LIGHT_NAVIGATION_BARS;
            getWindow().getInsetsController().setSystemBarsAppearance(light, light);
        }
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
        NativeHost.draw(host, time); requestFrame();
    }
    @Override protected void onStart() { super.onStart(); started = true; }
    @Override protected void onResume() { super.onResume(); resumed = true; activate(); }
    private void activate() {
        if (host != 0) {
            NativeHost.lifecycle(host, !started ? 3 : resumed && hasWindowFocus() ? 1 : 2);
            requestFrame();
        }
    }
    @Override public void onWindowFocusChanged(boolean focused) {
        super.onWindowFocusChanged(focused); if (!focused) stopFrame(); activate();
    }
    @Override protected void onPause() {
        resumed = false; stopFrame(); if (host != 0) NativeHost.lifecycle(host, 2);
        super.onPause();
    }
    @Override protected void onStop() {
        started = false; if (host != 0) NativeHost.lifecycle(host, 3); super.onStop();
    }
    @Override protected void onDestroy() {
        stopFrame(); surface.removeCallbacks(restoreSurface);
        if (Build.VERSION.SDK_INT >= 33)
            BackApi33.unregister(this, backCallback);
        if (receiverRegistered) unregisterReceiver(diagnostics);
        if (host != 0) { NativeHost.destroy(host); host = 0; }
        super.onDestroy();
    }
    private void handleBack() {
        if (host != 0 && NativeHost.back(host)) requestFrame();
        else finish();
    }
    @Override public void onBackPressed() { handleBack(); }
    @Override public void surfaceCreated(SurfaceHolder holder) { }
    @Override public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        stopFrame();
        available = host != 0 && NativeHost.attach(host, holder.getSurface(), width, height,
            getResources().getDisplayMetrics().density);
        error.setVisibility(available ? View.GONE : View.VISIBLE);
        if (!available) error.setText("OneUI 绘制表面创建失败。请返回后重新打开。\n" + NativeHost.status(host));
        requestFrame();
    }
    @Override public void surfaceDestroyed(SurfaceHolder holder) {
        available = false; stopFrame(); if (host != 0) NativeHost.detach(host);
    }
    private boolean touch(MotionEvent event) {
        if (host == 0 || !available || !resumed || !hasWindowFocus()) return false;
        int action = event.getActionMasked();
        if (event.getPointerCount() > 16 || action == MotionEvent.ACTION_CANCEL) NativeHost.cancel(host);
        else if (action == MotionEvent.ACTION_MOVE) {
            for (int i = 0; i < event.getPointerCount(); ++i) pointer(event, i, 1);
        } else if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) pointer(event, event.getActionIndex(), 0);
        else if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_POINTER_UP) pointer(event, event.getActionIndex(), 2);
        else return false;
        requestFrame(); return true;
    }
    private void pointer(MotionEvent event, int index, int phase) {
        int tool = event.getToolType(index);
        if (tool != MotionEvent.TOOL_TYPE_FINGER && tool != MotionEvent.TOOL_TYPE_STYLUS && tool != MotionEvent.TOOL_TYPE_ERASER) return;
        NativeHost.pointer(host, event.getPointerId(index), phase, tool == MotionEvent.TOOL_TYPE_FINGER ? 0 : 1,
            event.getX(index), event.getY(index), Math.max(0, Math.min(1, event.getPressure(index))), event.getEventTime() * 1000000L);
    }
}

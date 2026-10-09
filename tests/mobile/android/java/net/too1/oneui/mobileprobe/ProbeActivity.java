package net.too1.oneui.mobileprobe;

import android.app.Activity;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Build;
import android.view.Choreographer;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowInsetsController;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

// Test consumer for native lifecycle/GPU contracts; Android text and buttons
// are deliberately identified as host UI, not presented as OneUI controls.
public final class ProbeActivity extends Activity implements SurfaceHolder.Callback,
        Choreographer.FrameCallback {
    private long host;
    private SurfaceView surface;
    private TextView status;
    private boolean started, resumed, pending, available;
    private final Runnable restoreSurface = () -> {
        if (host != 0) surface.setVisibility(View.VISIBLE);
    };

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        host = NativeHost.create();
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(Color.rgb(250, 251, 249));
        root.setPadding(dp(20), dp(16), dp(20), dp(16));
        root.setOnApplyWindowInsetsListener((view, insets) -> {
            // Content does not overlap system bars; native Surface insets are 0.
            root.setPadding(dp(20) + insets.getSystemWindowInsetLeft(), dp(16) + insets.getSystemWindowInsetTop(),
                dp(20) + insets.getSystemWindowInsetRight(), dp(16) + insets.getSystemWindowInsetBottom());
            return insets;
        });
        TextView title = text("OneUI Android 宿主", 24);
        title.setAccessibilityHeading(true);
        root.addView(title);
        root.addView(text("拖动绘图区，松手后应停止绘制。切到后台再回来，位置应保留。", 15));
        surface = new SurfaceView(this);
        surface.setContentDescription("原生 EGL 绘制测试区；拖动可移动绿色方块");
        LinearLayout.LayoutParams fill = new LinearLayout.LayoutParams(-1, 0, 1);
        fill.setMargins(0, dp(16), 0, dp(12));
        root.addView(surface, fill);
        surface.getHolder().addCallback(this);
        surface.setOnTouchListener((view, event) -> touch(event));
        Button rebuild = new Button(this);
        rebuild.setText("重建绘制表面");
        rebuild.setMinHeight(dp(48));
        rebuild.setOnClickListener(view -> {
            surface.removeCallbacks(restoreSurface);
            surface.setVisibility(View.GONE);
            surface.postDelayed(restoreSurface, 150);
        });
        root.addView(rebuild, new LinearLayout.LayoutParams(-1, dp(52)));
        status = text("正在连接原生运行时…", 12);
        status.setTextIsSelectable(true);
        root.addView(status, new LinearLayout.LayoutParams(-1, dp(100)));
        root.addView(text("工程验证程序：EGL / 生命周期 / 触控。完整控件、输入法和无障碍桥尚在开发。", 12));
        setContentView(root);
        // setContentView creates DecorView, required by the insets controller.
        if (Build.VERSION.SDK_INT >= 30) {
            int lightBars = WindowInsetsController.APPEARANCE_LIGHT_STATUS_BARS |
                WindowInsetsController.APPEARANCE_LIGHT_NAVIGATION_BARS;
            getWindow().getInsetsController().setSystemBarsAppearance(lightBars, lightBars);
        } else {
            getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR | View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR);
        }
    }
    private int dp(int value) { return Math.round(value * getResources().getDisplayMetrics().density); }
    private TextView text(String value, int sp) {
        TextView view = new TextView(this);
        view.setText(value); view.setTextSize(sp); view.setTextColor(Color.rgb(35, 59, 44));
        view.setPadding(0, dp(4), 0, dp(4)); return view;
    }
    private void update() { if (host != 0) status.setText(NativeHost.status(host)); }
    private void requestFrame() {
        if (host != 0 && available && resumed && !pending && NativeHost.needsFrame(host)) {
            pending = true; Choreographer.getInstance().postFrameCallback(this);
        }
    }
    private void stopFrame() { Choreographer.getInstance().removeFrameCallback(this); pending = false; }
    @Override public void doFrame(long time) {
        pending = false;
        if (host == 0 || !available || !resumed) return;
        NativeHost.draw(host, time); update(); requestFrame();
    }
    @Override protected void onStart() { super.onStart(); started = true; }
    @Override protected void onResume() { super.onResume(); resumed = true; activate(); }
    private void activate() {
        if (host != 0) {
            NativeHost.lifecycle(host, !started ? 3 : resumed && hasWindowFocus() ? 1 : 2);
            update(); requestFrame();
        }
    }
    @Override public void onWindowFocusChanged(boolean focused) {
        super.onWindowFocusChanged(focused); if (!focused) stopFrame(); activate();
    }
    @Override protected void onPause() {
        resumed = false; stopFrame(); if (host != 0) NativeHost.lifecycle(host, 2);
        update(); super.onPause();
    }
    @Override protected void onStop() {
        started = false;
        if (host != 0) NativeHost.lifecycle(host, 3); update(); super.onStop();
    }
    @Override protected void onDestroy() {
        stopFrame(); surface.removeCallbacks(restoreSurface);
        if (host != 0) { NativeHost.destroy(host); host = 0; }
        super.onDestroy();
    }
    @Override public void surfaceCreated(SurfaceHolder holder) { /* dimensions arrive in surfaceChanged */ }
    @Override public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        stopFrame();
        available = host != 0 && NativeHost.attach(host, holder.getSurface(), width, height,
            getResources().getDisplayMetrics().density);
        update(); requestFrame();
    }
    @Override public void surfaceDestroyed(SurfaceHolder holder) {
        available = false; stopFrame(); if (host != 0) NativeHost.detach(host); update();
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

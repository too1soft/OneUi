package net.too1.oneui.mobile;

import android.app.Activity;
import android.content.Intent;
import android.content.Context;
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
public abstract class OneUiActivity extends Activity implements SurfaceHolder.Callback,
        Choreographer.FrameCallback {
    private long host;
    private EditorSurface surface;
    private NativeAccessibility accessibility;
    private TextView error;
    private SystemAppearance chrome;
    private boolean started, resumed, pending, available;
    private int editorInset;
    private float touchStartX, touchStartY;
    private boolean editorTap;
    private final Runnable timedWakeup = this::requestFrame;
    private Object backCallback;
    private final android.os.Handler servicesOwner = new android.os.Handler(android.os.Looper.getMainLooper());
    private final java.util.Map<Long, NativeRequest> inFlight = new java.util.HashMap<>();
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
    public interface Reply { void complete(int code, byte[] payload); }
    protected abstract String nativeLibrary();
    protected abstract void handleRequest(NativeRequest request, Reply reply);
    protected void servicesAttached() { }
    /** Product field semantics; the shared editor owns IME configuration. */
    protected int fieldInputType(String accessibleName) { return android.text.InputType.TYPE_CLASS_TEXT; }
    // Opt-in raw input for controls such as remote canvases. The product owns
    // capture/hit testing; ordinary widgets continue through the shared host.
    protected boolean interceptPointer(MotionEvent event) { return false; }
    protected void servicesDetached() { }
    protected boolean serviceActivityResult(int request, int result, Intent data) { return false; }
    protected final void event(byte[] payload) { deliver(0, 0, payload); }
    private void deliver(long request, int code, byte[] payload) {
        final byte[] owned = payload == null || payload.length > 1024 * 1024 ? new byte[0] : payload.clone();
        final int status = payload != null && payload.length > 1024 * 1024 ? -1 : code;
        servicesOwner.post(() -> {
            NativeRequest completed = inFlight.remove(request);
            if (completed != null) java.util.Arrays.fill(completed.payload, (byte)0);
            try {
                if (host == 0) return;
                NativeApplication.reply(host, request, status, owned);
                surface.sync(false); requestFrame(); drainRequests();
            } finally { java.util.Arrays.fill(owned, (byte)0); }
        });
    }
    private void drainRequests() {
        if (host == 0) return;
        for (NativeRequest request : NativeApplication.requests(host)) {
            if (inFlight.size() >= 16) { java.util.Arrays.fill(request.payload, (byte)0); deliver(request.id, -2, null); continue; }
            inFlight.put(request.id, request);
            java.util.concurrent.atomic.AtomicBoolean replied = new java.util.concurrent.atomic.AtomicBoolean();
			if (request.action.equals("oneui.open-url")) {
				try {
					String raw = new String(request.payload, java.nio.charset.StandardCharsets.UTF_8);
					java.net.URI uri = new java.net.URI(raw);
					if (raw.length() > 4096 || !"https".equals(uri.getScheme()) || uri.getHost() == null || uri.getRawUserInfo() != null) throw new IllegalArgumentException();
					startActivity(new Intent(Intent.ACTION_VIEW, android.net.Uri.parse(raw)).addCategory(Intent.CATEGORY_BROWSABLE));
					deliver(request.id, 0, new byte[0]);
				} catch (Exception error) { deliver(request.id, -4, new byte[0]); }
				continue;
			}
            try {
                handleRequest(request, (code, payload) -> {
                    if (replied.compareAndSet(false, true)) deliver(request.id, code, payload);
                });
            } catch (RuntimeException error) {
                if (replied.compareAndSet(false, true)) deliver(request.id, -1, new byte[0]);
            }
        }
    }

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        System.loadLibrary(nativeLibrary());
        servicesAttached();
        host = NativeApplication.create();
        chrome = new SystemAppearance(this, this::updateEditorInset);
        FrameLayout root = chrome.content();
        surface = new EditorSurface(this, new EditorSurface.Bridge() {
            public EditorSurface.Snapshot snapshot() { return host == 0 ? null : NativeApplication.editing(host); }
            public boolean apply(long session, String text, int a, int b, int start, int end) {
                return host != 0 && NativeApplication.edit(host, session, text, a, b, start, end);
            }
            public boolean sensitive(long session, int command, String text, int first, int second) {
                return host != 0 && NativeApplication.sensitiveEdit(host, session, command, text, first, second);
            }
            public boolean containsEditor(float x, float y) { return host != 0 && NativeApplication.containsEditor(host, x, y); }
            public EditorSurface.SelectionGeometry selectionGeometry() {
                return host == 0 ? null : NativeApplication.selectionGeometry(host);
            }
            public boolean selectAt(long session, int edge, float x, float y) {
                return host != 0 && NativeApplication.selectAt(host, session, edge, x, y);
            }
            public void cancelPointers() { if (host != 0) NativeApplication.cancel(host); }
            public int inputType(EditorSurface.Snapshot state) {
                if (host != 0) for (NativeUiNode node : NativeApplication.inspect(host))
                    if (node.focused && node.role == 3) return fieldInputType(node.name);
                return android.text.InputType.TYPE_CLASS_TEXT;
            }
            public void changed() { requestFrame(); }
        });
        surface.setContentDescription(getApplicationInfo().loadLabel(getPackageManager()));
        accessibility = new NativeAccessibility(surface, new NativeAccessibility.Bridge() {
            public NativeUiNode[] nodes() { return host == 0 ? new NativeUiNode[0] : NativeApplication.inspect(host); }
            public boolean activate(NativeUiNode node) {
                if(host==0 || !available)return false;
                float scale=getResources().getDisplayMetrics().density;
                float x=(node.x+node.width/2)*scale, y=(node.y+node.height/2)*scale;
                long time=android.os.SystemClock.uptimeMillis();
                MotionEvent down=MotionEvent.obtain(time,time,MotionEvent.ACTION_DOWN,x,y,0);
                MotionEvent up=MotionEvent.obtain(time,time,MotionEvent.ACTION_UP,x,y,0);
                down.setSource(android.view.InputDevice.SOURCE_TOUCHSCREEN);
                up.setSource(android.view.InputDevice.SOURCE_TOUCHSCREEN);
                boolean handled=touch(down) && touch(up);
                down.recycle();up.recycle();
                if(!handled)return false;
                if(node.role==3) {
                    boolean matched=false;
                    for(NativeUiNode current:NativeApplication.inspect(host))
                        if(current.focused && current.role==3 && current.name.equals(node.name))matched=true;
                    if(!matched)return false;
                }
                return true;
            }
            public boolean replaceText(String value) {
                EditorSurface.Snapshot state=host==0?null:NativeApplication.editing(host);
                if(state==null || state.session==0)return false;
                boolean result;
                if(state.sensitive) {
                    result=NativeApplication.sensitiveEdit(host,state.session,2,"",0,state.text.length()) &&
                        NativeApplication.sensitiveEdit(host,state.session,0,value,1,0);
                } else result=NativeApplication.edit(host,state.session,value,value.length(),value.length(),-1,-1);
                surface.sync(false);requestFrame();return result;
            }
            public boolean sensitive() {
                EditorSurface.Snapshot state=host==0?null:NativeApplication.editing(host);
                return state!=null && state.sensitive;
            }
            public String editorText() {
                EditorSurface.Snapshot state=host==0?null:NativeApplication.editing(host);
                return state!=null && !state.sensitive?state.text:"";
            }
            public void scroll(boolean forward) {
                if(host==0 || !available)return;
                float x=surface.getWidth()/2f,start=surface.getHeight()*(forward?.75f:.25f);
                float end=surface.getHeight()*(forward?.25f:.75f);
                long time=android.os.SystemClock.uptimeMillis()*1000000L;
                NativeApplication.pointer(host,31,0,0,x,start,1,time);
                NativeApplication.pointer(host,31,1,0,x,end,1,time);
                NativeApplication.pointer(host,31,2,0,x,end,0,time);
                surface.sync(false);requestFrame();
            }
        });
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
        drainRequests();
    }
    private void observeSystem() {
        NativeApplication.observeAppearance(host, chrome.systemTheme(), true);
        android.util.DisplayMetrics metrics = getResources().getDisplayMetrics();
        float body = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_SP, 16, metrics) / metrics.density;
        NativeApplication.typography(host, getResources().getConfiguration().fontScale, body);
    }
    private void syncAppearance() {
        if (host == 0 || chrome == null) return;
        long[] state = NativeApplication.appearance(host);
        surface.setSelectionHandleColor(state[6] == 2 ? 0xff8bb8ff : 0xff2468ee);
        // Android may restore theme bar flags after a configuration/focus change.
        // Reapply at these lifecycle boundaries even when native state is acknowledged.
        if (chrome.apply((int) state[2], (int) state[3],
                state[4] == 1, state[5] == 1, state[6] == 2 ? 0xff171e21 : 0xffffffff))
            NativeApplication.acknowledgeAppearance(host, state[0]);
    }
    @Override public void onConfigurationChanged(Configuration config) {
        super.onConfigurationChanged(config);
        if (host != 0) observeSystem();
        syncAppearance(); requestFrame();
        chrome.post(this::syncAppearance);
    }
    private void requestFrame() {
        servicesOwner.removeCallbacks(timedWakeup);
        if (host != 0 && available && resumed && !pending) {
            if (NativeApplication.needsFrame(host)) {
                pending = true; Choreographer.getInstance().postFrameCallback(this);
            } else {
                int delay = NativeApplication.wakeDelayMillis(host);
                if (delay >= 0) servicesOwner.postDelayed(timedWakeup, Math.max(1, delay));
            }
        }
    }
    private void stopFrame() {
        servicesOwner.removeCallbacks(timedWakeup);
        Choreographer.getInstance().removeFrameCallback(this); pending = false;
    }
    @Override public void doFrame(long time) {
        pending = false;
        if (host == 0 || !available || !resumed) return;
        if (!NativeApplication.draw(host, time)) {
            available = false; error.setText("页面暂时无法显示，请返回后重试。"); error.setVisibility(View.VISIBLE); return;
        }
        long[] appearance = NativeApplication.appearance(host);
        if (appearance[0] != appearance[1]) syncAppearance();
        surface.sync(false); drainRequests(); requestFrame();
        accessibility.changed();
    }
    @Override protected void onStart() { super.onStart(); started = true; }
    @Override protected void onResume() {
        super.onResume(); resumed = true;
        if (host != 0) observeSystem();
        syncAppearance(); activate();
    }
    private void activate() {
        if (host != 0) {
            // Window focus is independent of Activity lifetime. IMEs and
            // compatibility-host windows must not invalidate an active editor.
            NativeApplication.lifecycle(host, !started ? 3 : resumed ? 1 : 2);
            drainRequests();
            if (resumed) surface.resume(); else surface.suspend();
            requestFrame();
        }
    }
    @Override public void onWindowFocusChanged(boolean focused) {
        super.onWindowFocusChanged(focused);
        syncAppearance(); activate();
    }
    @Override protected void onPause() {
        resumed = false; surface.suspend(); stopFrame();
        if (host != 0) { NativeApplication.lifecycle(host, 2); drainRequests(); }
        super.onPause();
    }
    @Override protected void onStop() {
        started = false; if (host != 0) { NativeApplication.lifecycle(host, 3); drainRequests(); } super.onStop();
    }
    @Override protected void onDestroy() {
        stopFrame(); surface.suspend();
        if (host != 0) { NativeApplication.lifecycle(host, 2); drainRequests(); }
        servicesDetached();
        for (NativeRequest request : inFlight.values()) java.util.Arrays.fill(request.payload, (byte)0);
        inFlight.clear();
        if (Build.VERSION.SDK_INT >= 33)
            BackApi33.unregister(this, backCallback);
        if (host != 0) { NativeApplication.destroy(host); host = 0; }
        super.onDestroy();
    }
    private void handleBack() {
        if (surface != null && surface.dismissEditingActions()) return;
        if (host != 0 && NativeApplication.back(host)) { syncAppearance(); requestFrame(); }
        else finish();
    }
    @Override public void onBackPressed() { handleBack(); }
    @Override public void surfaceCreated(SurfaceHolder holder) { }
    private void updateEditorInset(int value) {
        if (editorInset == value) return;
        editorInset = value;
        if (host != 0 && available) {
            NativeApplication.editorInset(host, value);
            requestFrame();
        }
    }
    @Override public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        surface.cancelSelectionGesture();
        stopFrame();
        available = host != 0 && NativeApplication.attach(host, holder.getSurface(), width, height,
            getResources().getDisplayMetrics().density);
        if (available) NativeApplication.editorInset(host, editorInset);
        error.setVisibility(available ? View.GONE : View.VISIBLE);
        if (!available) error.setText("页面暂时无法显示，请返回后重试。");
        syncAppearance();
        requestFrame();
    }
    @Override public void surfaceDestroyed(SurfaceHolder holder) {
        available = false; stopFrame(); if (host != 0) NativeApplication.detach(host);
    }
    private boolean touch(MotionEvent event) {
        if (host == 0 || !available || !resumed || !hasWindowFocus()) return false;
        if (interceptPointer(event)) { requestFrame(); drainRequests(); return true; }
        if (surface.beforeEditorTouch(event)) { requestFrame(); return true; }
        int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_DOWN) {
            touchStartX = event.getX(); touchStartY = event.getY(); editorTap = true;
        } else if (action == MotionEvent.ACTION_MOVE) {
            float slop = android.view.ViewConfiguration.get(this).getScaledTouchSlop();
            if (Math.hypot(event.getX() - touchStartX, event.getY() - touchStartY) > slop) editorTap = false;
        } else if (action == MotionEvent.ACTION_CANCEL || event.getPointerCount() > 1) editorTap = false;
        if (event.getPointerCount() > 16 || action == MotionEvent.ACTION_CANCEL) NativeApplication.cancel(host);
        else if (action == MotionEvent.ACTION_MOVE) {
            for (int i = 0; i < event.getPointerCount(); ++i) pointer(event, i, 1);
        } else if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) pointer(event, event.getActionIndex(), 0);
        else if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_POINTER_UP) pointer(event, event.getActionIndex(), 2);
        else return false;
        surface.sync(false);
        boolean editingMenu = surface.onEditorTouch(event);
        if (action == MotionEvent.ACTION_UP && editorTap && !editingMenu &&
                NativeApplication.containsEditor(host, event.getX(), event.getY())) surface.sync(true);
        syncAppearance(); drainRequests(); requestFrame(); return true;
    }
    @Override protected void onActivityResult(int request, int result, Intent data) {
        if (!serviceActivityResult(request, result, data)) super.onActivityResult(request, result, data);
    }
    private void pointer(MotionEvent event, int index, int phase) {
        int tool = event.getToolType(index);
        boolean pen = tool == MotionEvent.TOOL_TYPE_STYLUS || tool == MotionEvent.TOOL_TYPE_ERASER;
        boolean touch = tool == MotionEvent.TOOL_TYPE_FINGER || tool == MotionEvent.TOOL_TYPE_MOUSE ||
            (tool == MotionEvent.TOOL_TYPE_UNKNOWN && event.isFromSource(android.view.InputDevice.SOURCE_TOUCHSCREEN));
        if (!pen && !touch) return;
        if (tool == MotionEvent.TOOL_TYPE_MOUSE && phase == 0 &&
                (event.getButtonState() & MotionEvent.BUTTON_SECONDARY) != 0 &&
                (event.getButtonState() & MotionEvent.BUTTON_PRIMARY) == 0) return;
        NativeApplication.pointer(host, event.getPointerId(index), phase, pen ? 1 : 0,
            event.getX(index), event.getY(index), Math.max(0, Math.min(1, event.getPressure(index))), event.getEventTime() * 1000000L);
    }
}

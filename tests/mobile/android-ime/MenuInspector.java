package net.too1.oneui.inputtests;

import android.accessibilityservice.AccessibilityServiceInfo;
import android.app.Activity;
import android.app.Instrumentation;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.Rect;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.SystemClock;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityWindowInfo;
import org.json.JSONArray;
import org.json.JSONObject;

/** Test-only inspector: enumerate real floating windows missed by `uiautomator
 * dump`. Reports only fixed edit-action labels and bounds, never editor text.
 * The external driver injects real pointer events; no menu command is invoked here.
 */
public final class MenuInspector extends Instrumentation {
    private volatile boolean done;
    @Override public void onCreate(Bundle args) { super.onCreate(args); start(); }
    private void collect(AccessibilityNodeInfo node, JSONArray result) throws Exception {
        if (node == null) return;
        CharSequence value = node.getText();
        String text = value == null ? "" : value.toString().toLowerCase(java.util.Locale.ROOT);
        if ("android.view.SurfaceView".contentEquals(node.getClassName())) text = "__surface__";
        if (node.isVisibleToUser() && (text.equals("select all") || text.equals("copy") ||
            text.equals("cut") || text.equals("paste") || text.equals("__surface__"))) {
            Rect rect = new Rect(); node.getBoundsInScreen(rect);
            JSONObject item = new JSONObject(); item.put("text", text);
            item.put("bounds", new JSONArray(new int[]{rect.left, rect.top, rect.right, rect.bottom}));
            result.put(item);
        }
        for (int i = 0; i < node.getChildCount(); ++i) collect(node.getChild(i), result);
        node.recycle();
    }
    @Override public void onStart() {
        HandlerThread worker = new HandlerThread("OneUI menu fixture"); worker.start();
        BroadcastReceiver receiver = new BroadcastReceiver() {
            @Override public void onReceive(Context context, Intent intent) {
                if ("quit".equals(intent.getStringExtra("command"))) { done = true; return; }
                JSONArray result = new JSONArray();
                try {
                    for (AccessibilityWindowInfo window : getUiAutomation().getWindows()) collect(window.getRoot(), result);
                    setResultData(result.toString());
                } catch (Exception error) { setResultData("ERROR:" + error.getClass().getSimpleName()); }
            }
        };
        Bundle result = new Bundle();
        try {
            AccessibilityServiceInfo info = getUiAutomation().getServiceInfo();
            info.flags |= AccessibilityServiceInfo.FLAG_RETRIEVE_INTERACTIVE_WINDOWS;
            getUiAutomation().setServiceInfo(info);
            getTargetContext().registerReceiver(receiver, new IntentFilter("net.too1.oneui.inputtests.MENU"),
                "android.permission.DUMP", new Handler(worker.getLooper()), Context.RECEIVER_EXPORTED);
            Intent launch = getTargetContext().getPackageManager().getLaunchIntentForPackage("net.too1.oneui.lab");
            launch.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK); startActivitySync(launch);
            long end = SystemClock.uptimeMillis() + 240000;
            while (!done && SystemClock.uptimeMillis() < end) SystemClock.sleep(100);
            result.putString("stream", done ? "PASS: actual floating-window inspection\n" : "FAIL: driver timeout\n");
        } catch (Throwable error) { result.putString("stream", "FAIL:" + error.toString()); }
        finally {
            try { getTargetContext().unregisterReceiver(receiver); } catch (IllegalArgumentException ignored) { }
            worker.quitSafely();
        }
        finish(done ? Activity.RESULT_OK : Activity.RESULT_CANCELED, result);
    }
}

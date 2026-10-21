package net.too1.oneui.inputtests;
import android.app.Activity;
import android.app.Instrumentation;
import android.content.Intent;
import android.os.Bundle;
import android.os.SystemClock;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.ExtractedTextRequest;
import net.too1.oneui.EditorSurface;

/** Protocol fixture: the external driver navigates/taps the real C++ editor.
 * This exercises the actual Android InputConnection/JNI, not a debug text setter.
 * It is not evidence of a Chinese candidate-window interaction.
 */
public final class InputProtocol extends Instrumentation {
    private boolean sensitive;
    private volatile boolean driverReady;
    private volatile EditorSurface currentSurface;
    private final android.content.BroadcastReceiver driver = new android.content.BroadcastReceiver() {
        @Override public void onReceive(android.content.Context context, Intent intent) {
            if ("surface".equals(intent.getStringExtra("command"))) {
                EditorSurface surface = currentSurface;
                if (surface != null && surface.isShown() && surface.getWidth() > 0 && surface.getHeight() > 0) {
                    int[] p = new int[2]; surface.getLocationOnScreen(p);
                    setResultData("["+p[0]+","+p[1]+","+(p[0]+surface.getWidth())+","+(p[1]+surface.getHeight())+"]");
                }
            } else driverReady = true;
        }
    };
    @Override public void onCreate(Bundle args) {
        super.onCreate(args); sensitive = args != null && "secure".equals(args.getString("mode")); start();
    }
    static void check(boolean value, String why) { if (!value) throw new AssertionError(why); }
    static EditorSurface find(View view) {
        if (view instanceof EditorSurface) return (EditorSurface)view;
        if (view instanceof ViewGroup) {
            ViewGroup group = (ViewGroup)view;
            for (int i=0; i<group.getChildCount(); ++i) { EditorSurface found=find(group.getChildAt(i)); if(found!=null)return found; }
        }
        return null;
    }
    String text(InputConnection c) { return c.getExtractedText(new ExtractedTextRequest(), 0).text.toString(); }
    void verify(EditorSurface surface) {
        InputConnection c = surface.onCreateInputConnection(new EditorInfo());
        check(c != null, "No real connection");
        check(c.commitText("ab", 1), "commit");
        check(c.setSelection(1, 1), "selection");
        check(c.setComposingText("zh", 1) && text(c).equals("azhb"), "Latin preedit");
        check(c.setComposingText("中文", 1) && text(c).equals("a中文b"), "Chinese preedit");
        check(c.commitText("中文", 1) && text(c).equals("a中文b"), "Chinese commit");
        check(c.setSelection(1, 3) && c.getSelectedText(0).toString().equals("中文"), "native selection");
        check(c.commitText("😀", 1) && text(c).equals("a😀b"), "emoji replacement");
        check(c.deleteSurroundingTextInCodePoints(1, 0) && text(c).equals("ab"), "codepoint deletion");
        check(c.setComposingRegion(0, 1), "composing region");
        check(c.setComposingText("一", 1) && c.finishComposingText() && text(c).equals("一b"), "region replacement");
        check(c.beginBatchEdit(), "batch start");
        check(c.setSelection(0, 2) && c.commitText("batch", 1), "batch replace");
        c.endBatchEdit(); check(text(c).equals("batch"), "batch final");
        check(c.setSelection(4, 1) && c.getSelectedText(0).toString().equals("atc"), "reversed selection");
        check(c.commitText("Z", 1) && text(c).equals("bZh"), "reverse replace");
        check(!c.commitText("\ud800", 1), "unpaired surrogate accepted");
        check(!c.commitText("stale", 1), "closed connection accepted writes");
        surface.sync(false);
        InputConnection replacement = surface.onCreateInputConnection(new EditorInfo());
        check(text(replacement).equals("bZh"), "invalid input corrupted native text");
        surface.suspend(); check(!replacement.commitText("late", 1), "suspended callback accepted");
        surface.resume();
    }
    @Override public void onStart() {
        Bundle result = new Bundle();
        try {
            if (sensitive) getTargetContext().registerReceiver(driver,
                new android.content.IntentFilter("net.too1.oneui.inputtests.RUN"),
                "android.permission.DUMP", null, android.content.Context.RECEIVER_EXPORTED);
            Intent launch = getTargetContext().getPackageManager().getLaunchIntentForPackage("net.too1.oneui.lab");
            launch.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            Activity activity = startActivitySync(launch);
            EditorSurface[] found = new EditorSurface[1]; boolean[] ready = {false};
            long driverDeadline = SystemClock.uptimeMillis() + 180000;
            while (!ready[0] && SystemClock.uptimeMillis() < driverDeadline) {
                runOnMainSync(() -> { found[0] = find(activity.getWindow().getDecorView());
                    currentSurface = found[0];
                    ready[0] = found[0]!=null && found[0].onCheckIsTextEditor();
                    if (ready[0] && sensitive && driverReady) {
                        EditorInfo info = new EditorInfo();
                        found[0].onCreateInputConnection(info);
                        ready[0] = (info.inputType & android.text.InputType.TYPE_MASK_VARIATION) ==
                            android.text.InputType.TYPE_TEXT_VARIATION_PASSWORD;
                    } else if (sensitive) ready[0] = false;
                });
                if (!ready[0]) SystemClock.sleep(200);
            }
            check(ready[0], "Driver did not focus the real text page within 180 seconds");
            Throwable[] failure = new Throwable[1];
            runOnMainSync(() -> { try {
                if (sensitive) SecureChecks.verify(found[0]); else verify(found[0]);
            } catch(Throwable t) { failure[0]=t; } });
            if (failure[0]!=null) throw new AssertionError(failure[0]);
            result.putString("stream", sensitive ? "PASS: password InputConnection privacy, commands, clipboard, stale/suspend\n" :
                "PASS: actual InputConnection/JNI Chinese preedit, emoji, selection, batch, invalid/stale/suspend\n");
            if (sensitive) getTargetContext().unregisterReceiver(driver);
            finish(Activity.RESULT_OK, result);
        } catch(Throwable failure) {
            if (sensitive) getTargetContext().unregisterReceiver(driver);
            result.putString("stream", "FAIL: "+failure.toString()+"\n");
            finish(Activity.RESULT_CANCELED, result);
        }
    }
}

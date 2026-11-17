package net.too1.oneui.mobile;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import java.nio.charset.StandardCharsets;

/** Optional system text clipboard bridge. The owner controls foreground/focus policy. */
public final class PlainTextClipboard implements AutoCloseable {
    public enum Status { OK, EMPTY, UNSUPPORTED, TOO_LARGE, UNAVAILABLE }
    public static final class Result {
        public final Status status;
        public final String text;
        Result(Status status, String text) { this.status = status; this.text = text; }
    }

    private final ClipboardManager clipboard;
    private final int maxBytes;
    private final ClipboardManager.OnPrimaryClipChangedListener listener;
    private boolean watching;

    public PlainTextClipboard(Context context, int maxBytes, Runnable changed) {
        if (maxBytes < 1 || changed == null) throw new IllegalArgumentException("Clipboard configuration is required");
        this.clipboard = (ClipboardManager) context.getApplicationContext().getSystemService(Context.CLIPBOARD_SERVICE);
        this.maxBytes = maxBytes;
        this.listener = changed::run;
    }

    public Result read() {
        try {
            ClipData clip = clipboard == null ? null : clipboard.getPrimaryClip();
            if (clip == null || clip.getItemCount() == 0) return new Result(Status.EMPTY, "");
            // Do not dereference content URIs or invoke a provider to coerce non-text data.
            CharSequence value = clip.getItemAt(0).getText();
            if (value == null) return new Result(Status.UNSUPPORTED, "");
            if (value.length() > maxBytes) return new Result(Status.TOO_LARGE, "");
            String text = value.toString();
            if (text.getBytes(StandardCharsets.UTF_8).length > maxBytes) return new Result(Status.TOO_LARGE, "");
            return new Result(Status.OK, text);
        } catch (RuntimeException denied) {
            return new Result(Status.UNAVAILABLE, "");
        }
    }

    public boolean write(String label, String text) {
        if (clipboard == null || text == null || text.length() > maxBytes ||
            text.getBytes(StandardCharsets.UTF_8).length > maxBytes) return false;
        try {
            clipboard.setPrimaryClip(ClipData.newPlainText(label, text));
            return true;
        } catch (RuntimeException unavailable) { return false; }
    }

    public void watch(boolean enabled) {
        if (clipboard == null || enabled == watching) return;
        if (enabled) clipboard.addPrimaryClipChangedListener(listener);
        else clipboard.removePrimaryClipChangedListener(listener);
        watching = enabled;
    }

    @Override public void close() { watch(false); }
}

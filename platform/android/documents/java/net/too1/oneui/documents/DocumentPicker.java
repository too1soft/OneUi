package net.too1.oneui.documents;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.provider.DocumentsContract;
import java.util.function.BiConsumer;

/** Android's own folder chooser. The consumer handles selection work off the UI thread. */
public final class DocumentPicker {
    private final Activity activity;
    private final int requestCode;
    private BiConsumer<Uri, Integer> completion;
    public DocumentPicker(Activity activity, int requestCode) { this.activity = activity; this.requestCode = requestCode; }
    public boolean choose(BiConsumer<Uri, Integer> completion) {
        if (this.completion != null) return false;
        this.completion = completion;
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE).addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION |
            Intent.FLAG_GRANT_WRITE_URI_PERMISSION | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION | Intent.FLAG_GRANT_PREFIX_URI_PERMISSION);
        try { activity.startActivityForResult(intent, requestCode); return true; }
        catch (RuntimeException error) { this.completion = null; return false; }
    }
    public boolean result(int requestCode, int resultCode, Intent data) {
        if (requestCode != this.requestCode || completion == null) return false;
        BiConsumer<Uri, Integer> callback = completion; completion = null;
        callback.accept(resultCode == Activity.RESULT_OK && data != null ? data.getData() : null,
            data == null ? 0 : data.getFlags());
        return true;
    }
}
